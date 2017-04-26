#include <cassert>
#include <iomanip>
#include <set>
#include <map>
#include <unordered_map>
#include <utility>
#include <iostream>
#include <algorithm>
#include <glm/ext.hpp>
#include <vtkDataSetAttributes.h>
#include <vtkDataSet.h>
#include <vtkIdList.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkDataArray.h>
#include "imgui-1.49/imgui.h"
#include "tree_widget.h"

std::ostream& operator<<(std::ostream &os, const Branch &b) {
	os << "Branch {\n\tsegmentation_id: " << b.segmentation_id
		<< "\n\tstart: " << glm::to_string(b.start)
		<< "\n\tend: " << glm::to_string(b.end)
		<< "\n\tstart_node: " << b.start_node
		<< "\n\tend_node: " << b.end_node
		<< "\n\tstart_val: " << b.start_val
		<< "\n\tend_val: " << b.end_val << "\n}";
	return os;
}

glm::vec2 TreeNode::get_input_slot_pos(const size_t segment) const {
	auto fnd = std::find(entering_branches.begin(), entering_branches.end(), segment);
	assert(fnd != entering_branches.end());
	const size_t slot = std::distance(entering_branches.begin(), fnd);
	return glm::vec2(ui_pos.x, ui_pos.y + ui_size.y * static_cast<float>(slot + 1) / (entering_branches.size() + 1));
}
glm::vec2 TreeNode::get_output_slot_pos(const size_t segment) const {
	auto fnd = std::find(exiting_branches.begin(), exiting_branches.end(), segment);
	assert(fnd != exiting_branches.end());
	const size_t slot = std::distance(exiting_branches.begin(), fnd);
	return glm::vec2(ui_pos.x + ui_size.x,
			ui_pos.y + ui_size.y * static_cast<float>(slot + 1) / (exiting_branches.size() + 1));
}
std::ostream& operator<<(std::ostream &os, const TreeNode &n) {
	os << "TreeNode {\n\tid: " << n.node_id << "\n\ttype: " << n.type
		<< "\n\tpos: " << glm::to_string(n.pos)
		<< "\n\tval: " << n.value;

	os << "\n\tentering: [";
	for (const auto &x : n.entering_branches) {
		os << x << ", ";
	}
	os << "]\n\texiting: [";
	for (const auto &x : n.exiting_branches) {
		os << x << ", ";
	}
	os << "]\n}";
	return os;
}

TreeWidget::TreeWidget(vtkSmartPointer<vtkContourForests> cf, vtkTopologicalSimplification *simplification)
	: contour_forest(cf), tree_type(ttk::TreeType::Split), tree_arcs(nullptr), tree_nodes(nullptr),
	zoom_amount(1.f), scrolling(0.f)
{
	// Watch for updates to the contour forest
	cf->SetTreeType(tree_type);
	cf->Update();
	build_tree();
	cf->AddObserver(vtkCommand::EndEvent, this);
	simplification->AddObserver(vtkCommand::EndEvent, this);
}
bool point_on_line(const glm::vec2 &start, const glm::vec2 &end, const glm::vec2 &point) {
	const float click_dist = 4;
	if (point.x < std::min(start.x, end.x) - click_dist || point.x > std::max(start.x, end.x) + click_dist
			|| point.y < std::min(start.y, end.y) - click_dist || point.y > std::max(start.y, end.y) + click_dist) {
		return false;
	}
	const glm::vec2 dir = end - start;
	const glm::vec2 n = glm::normalize(glm::vec2(dir.y, -dir.x));
	const float d = std::abs(glm::dot(n, point - start));
	// Measurements are in pixels
	return d < click_dist;
}
void TreeWidget::draw_ui() {
	if (!ImGui::Begin("Tree Widget")) {
		ImGui::End();
		return;
	}

	int tree_selection = 0;
	switch (tree_type) {
		case ttk::TreeType::Contour: tree_selection = 0; break;
		case ttk::TreeType::Split: tree_selection = 1; break;
		case ttk::TreeType::Join: tree_selection = 2; break;
		default: tree_selection = 0;
	}
	ImGui::Text("Tree Type");
	ImGui::RadioButton("Contour", &tree_selection, 0); ImGui::SameLine();
	ImGui::RadioButton("Split", &tree_selection, 1); ImGui::SameLine();
	ImGui::RadioButton("Join", &tree_selection, 2);

	if (branches.empty() || nodes.empty()){
		ImGui::Text("Tree is empty!");
		ImGui::End();
		return;
	}

	if (ImGui::Button("Clear Selection")) {
		selected_segmentations.clear();
	}

	// This is based on the material editor node-link diagram from
	// https://gist.github.com/ocornut/7e9b3ec566a333d725d4
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, glm::vec2(1));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, glm::vec2(0));
	ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(60, 60, 70, 200));
	ImGui::BeginChild("tree_region", glm::vec2(0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
	ImGui::PushItemWidth(120.f);

	const glm::vec2 offset = glm::vec2(ImGui::GetCursorScreenPos()) - scrolling;
	const glm::vec2 mouse_pos = glm::vec2(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	draw_list->ChannelsSplit(2);

	// Draw the nodes in the foreground
	bool node_hovered = false;
	const glm::vec2 NODE_WINDOW_PADDING(8);
	draw_list->ChannelsSetCurrent(1);
	for (size_t i = 0; i < nodes.size(); ++i) {
		TreeNode &n = nodes[i];
		ImGui::PushID(i);

		// Draw the node rect
		const glm::vec2 rect_start = offset + n.ui_pos;
		ImGui::SetCursorScreenPos(rect_start);
		ImGui::InvisibleButton("node", n.ui_size);
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
			n.ui_pos += glm::vec2(ImGui::GetIO().MouseDelta);
		}
		node_hovered |= ImGui::IsItemHovered();

		const glm::vec2 rect_end = rect_start + n.ui_size;
		draw_list->AddRectFilled(rect_start, rect_end, ImColor(60, 60, 60), 4.f);
		draw_list->AddRect(rect_start, rect_end, ImColor(100, 100, 100), 4.f);

		// Draw little circles for each connection on to the node
		for (const auto &x : n.entering_branches) {
			draw_list->AddCircleFilled(offset + n.get_input_slot_pos(x), 5.f, ImColor(150, 150, 150, 150));
		}
		for (const auto &x : n.exiting_branches) {
			draw_list->AddCircleFilled(offset + n.get_output_slot_pos(x), 5.f, ImColor(150, 150, 150, 150));
		}

		// Draw node rect text on top
		ImGui::SetCursorScreenPos(rect_start + NODE_WINDOW_PADDING);
		ImGui::BeginGroup();
		const char *type = nullptr;
		switch (n.type) {
			case 0: type = "Minima"; break;
			case 1: type = "1-Saddle"; break;
			case 2: type = "2-Saddle"; break;
			case 3: type = "Maxima"; break;
			default: type = "Unknown"; break;
		}
		ImGui::Text("Node %lu\nType: %s\nValue: %.2f", n.node_id, type, n.value);
		ImGui::EndGroup();

		ImGui::PopID();
	}

	// Display the graph links, in background
	draw_list->ChannelsSetCurrent(0);
	size_t branch_selection = -1;
	for (const auto &b : branches) {
		if (b.start_node >= nodes.size() || b.end_node >= nodes.size()) {
			std::cout << "Bad branch connection\n";
		}
		const TreeNode &start = nodes[b.start_node];
		const TreeNode &end = nodes[b.end_node];
		const glm::vec2 p1 = offset + start.get_output_slot_pos(b.segmentation_id);
		const glm::vec2 p2 = offset + end.get_input_slot_pos(b.segmentation_id);

		const bool selected = std::find(selected_segmentations.begin(), selected_segmentations.end(), b.segmentation_id)
			!= selected_segmentations.end();
		const ImColor color = selected ? ImColor(0.9f, 0.9f, 0.2f, 1.f) : ImColor(0.8f, 0.8f, 0.1f, 0.5f);
		draw_list->AddLine(p1, p2, color, 2.f);

		if (ImGui::IsWindowHovered() && point_on_line(p1, p2, mouse_pos)) {
			if (!node_hovered) {
				ImGui::SetTooltip("Segment %lu", b.segmentation_id);
			}
			if (ImGui::IsMouseClicked(0)) {
				branch_selection = b.segmentation_id;
			}
		}
	}

	draw_list->ChannelsMerge();

	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()) {
		if (ImGui::IsMouseDragging(2, 0.f)) {
			scrolling -= glm::vec2(ImGui::GetIO().MouseDelta);
		} else if (branch_selection != size_t(-1)) {
			auto fnd = std::find(selected_segmentations.begin(), selected_segmentations.end(), branch_selection);
			const bool was_selected = fnd != selected_segmentations.end();
			if (!ImGui::GetIO().KeyCtrl) {
				selected_segmentations.clear();
				fnd = selected_segmentations.end();
			}

			if (fnd != selected_segmentations.end()) {
				selected_segmentations.erase(fnd);
			} else if (!was_selected) {
				selected_segmentations.push_back(branch_selection);
			}
		}
	}

	ImGui::PopItemWidth();
	ImGui::EndChild();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	ImGui::End();

	// Update the tree if we chose a new type
	int new_tree_type = tree_type;
	switch (tree_selection) {
		case 0: new_tree_type = ttk::TreeType::Contour; break;
		case 1: new_tree_type = ttk::TreeType::Split; break;
		case 2: new_tree_type = ttk::TreeType::Join; break;
		default: break;
	}
	if (new_tree_type != tree_type) {
		tree_type = new_tree_type;
		contour_forest->SetTreeType(tree_type);
		contour_forest->Update();
	}
}
const std::vector<uint32_t>& TreeWidget::get_selection() const {
	return selected_segmentations;
}
void TreeWidget::Execute(vtkObject *caller, unsigned long event_id, void *call_data) {
	// If the contour forest filter called us, update the tree. Otherwise the simplification changed
	// and we should recompute the tree now.
	if (caller == contour_forest.Get()) {
		build_tree();
	} else {
		// It seems that updating the contour forest re-updates the simplification (who just called us),
		// so remove us as an observer before updating CF, then re-add us.
		caller->RemoveObserver(this);
		contour_forest->Update();
		caller->AddObserver(event_id, this);
	}
}
ttk::TreeType TreeWidget::get_tree_type() const {
	switch (tree_type) {
		case 0: return ttk::TreeType::Contour;
		case 1: return ttk::TreeType::Split;
		case 2: return ttk::TreeType::Join;
		default: return ttk::TreeType::Contour;
	}
}
void TreeWidget::build_tree() {
	branches.clear();
	nodes.clear();
	selected_segmentations.clear();
	zoom_amount = 1.0;
	scrolling = glm::vec2(0);

	tree_nodes = dynamic_cast<vtkPolyData*>(contour_forest->GetOutput(0));
	tree_arcs = dynamic_cast<vtkPolyData*>(contour_forest->GetOutput(1));
	assert(tree_nodes);
	assert(tree_arcs);

	vtkDataSetAttributes *point_attribs = tree_arcs->GetAttributes(vtkDataSet::POINT);
	vtkPoints *points = tree_arcs->GetPoints();

	vtkDataSetAttributes *cell_attribs = tree_arcs->GetAttributes(vtkDataSet::CELL);
	vtkCellArray *lines = tree_arcs->GetLines();
	if (!points || !lines) {
		std::cout << "Empty selection\n";
		return;
	}

	int idx = 0;
	vtkDataArray *pt_img_file = point_attribs->GetArray("ImageFile", idx);
	vtkDataArray *line_seg_id = cell_attribs->GetArray("SegmentationId", idx);
	std::cout << "There are " << std::fixed << line_seg_id->GetRange()[1] + 1 << " Unique segmentation ids\n";
	branches.resize(size_t(line_seg_id->GetRange()[1]) + 1, Branch());
	std::cout << "# of lines = " << lines->GetNumberOfCells() << "\n";

	Branch current_branch;
	current_branch.start_val = pt_img_file->GetTuple(0)[0];
	current_branch.end_val = pt_img_file->GetTuple(1)[0];
	current_branch.segmentation_id = line_seg_id->GetTuple(0)[0];
	for (size_t i = 0; i < lines->GetNumberOfCells(); ++i) {
		const size_t seg = line_seg_id->GetTuple(i)[0];
		double pt_pos[3];
		points->GetPoint(i * 2, pt_pos);
		const glm::uvec3 start_pos = glm::uvec3(pt_pos[0], pt_pos[1], pt_pos[2]);
		points->GetPoint(i * 2 + 1, pt_pos);
		const glm::uvec3 end_pos = glm::uvec3(pt_pos[0], pt_pos[1], pt_pos[2]);

		const float start_val = pt_img_file->GetTuple(i * 2)[0];
		const float end_val = pt_img_file->GetTuple(i * 2 + 1)[0];

		// If this line is part of a new segmentation end our current one
		if (current_branch.segmentation_id != seg) {
			branches[current_branch.segmentation_id] = current_branch;

			current_branch = Branch();
			current_branch.start = start_pos;
			current_branch.start_val = start_val;
			current_branch.segmentation_id = seg;
		}
		current_branch.end = end_pos;
		current_branch.end_val = end_val;
		// TODO: In a case with 2 nodes and 1 branch, we don't set the start/end nodes?
		current_branch.start_node = -1;
		current_branch.end_node = -1;
	}
	// Push on the last segmentation
	branches[current_branch.segmentation_id] = current_branch;

	// Setup node data
	vtkPoints *node_points = tree_nodes->GetPoints();
	vtkDataSetAttributes *node_attribs = tree_nodes->GetAttributes(vtkDataSet::POINT);
	// Find the number of unique imagefile vals so we can compress the tree down a bit
	std::set<float> node_img_vals;
	for (size_t i = 0; i < node_points->GetNumberOfPoints(); ++i) {
		node_img_vals.insert(node_attribs->GetArray("ImageFile", idx)->GetTuple(i)[0]);
	}

	// Build the list of nodes and their connections in the tree
	std::unordered_map<float, size_t> node_val_count;
	const glm::vec2 node_dims(116, 60);
	const glm::vec2 node_spacing(24, 12);
	std::cout << "# of node points = " << node_points->GetNumberOfPoints() << "\n";
	for (size_t i = 0; i < node_points->GetNumberOfPoints(); ++i) {
		int idx = 0;
		TreeNode n;
		double pt_pos[3];
		node_points->GetPoint(i, pt_pos);
		n.node_id = node_attribs->GetArray("NodeIdentifier", idx)->GetTuple(i)[0];
		n.pos = glm::uvec3(pt_pos[0], pt_pos[1], pt_pos[2]);
		n.value = node_attribs->GetArray("ImageFile", idx)->GetTuple(i)[0];
		n.type = node_attribs->GetArray("NodeType", idx)->GetTuple(i)[0];

		// Compute node position in the ui layout
		auto fnd = node_img_vals.find(n.value);
		n.ui_pos = glm::vec2(std::distance(node_img_vals.begin(), fnd) * (node_dims.x + node_spacing.x),
				node_val_count[n.value] * (node_dims.y + node_spacing.y));
		n.ui_size = node_dims;

		node_val_count[n.value]++;

		for (auto &b : branches) {
			if (b.start == n.pos) {
				n.exiting_branches.push_back(b.segmentation_id);
				b.start_node = i;
			} else if (b.end == n.pos) {
				n.entering_branches.push_back(b.segmentation_id);
				b.end_node = i;
			}
		}
		nodes.push_back(n);
	}
	std::cout << "Built nodes" << std::endl;

	// Go through all branches and make sure we aren't missing any nodes, sometimes
	// the node positions don't seem to be in the right spot.
	for (auto &b : branches) {
		if (b.start_node >= nodes.size()) {
			// Find the node it should be connected too, hopefully
			auto fnd = std::find_if(nodes.begin(), nodes.end(),
					[&](const TreeNode &n) { return std::abs(n.value - b.start_val) < 0.001; });
			if (fnd != nodes.end()) {
				b.start_node = std::distance(nodes.begin(), fnd);
				fnd->exiting_branches.push_back(b.segmentation_id);
			}
		}
		if (b.end_node >= nodes.size()) {
			// Find the node it should be connected too, hopefully
			auto fnd = std::find_if(nodes.begin(), nodes.end(),
					[&](const TreeNode &n) { return std::abs(n.value - b.end_val) < 0.001; });
			if (fnd != nodes.end()) {
				b.end_node = std::distance(nodes.begin(), fnd);
				fnd->entering_branches.push_back(b.segmentation_id);
			}
		}
	}
	std::cout << "Ui graph is done being built" << std::endl;

	// Sometimes on Tooth the node/branches don't seem to be constructed properly
	// for us to re-link them
	auto node0 = std::find_if(nodes.begin(), nodes.end(), [](const TreeNode &n) { return n.node_id == 0; });
	if (node0 != nodes.end() && branches[0].start_node >= nodes.size() && node0->exiting_branches.empty()) {
		node0->exiting_branches.push_back(branches[0].segmentation_id);
		branches[0].start_node = std::distance(nodes.begin(), node0);
	}
}

