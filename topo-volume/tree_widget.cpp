#include <cassert>
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
		<< "\n\tstart_val: " << b.start_val
		<< "\n\tend_val: " << b.end_val;

	os << "\n\tentering: [";
	for (const auto &x : b.entering_branches) {
		os << x << ", ";
	}
	os << "]\n\texiting: [";
	for (const auto &x : b.exiting_branches) {
		os << x << ", ";
	}
	os << "]\n}";
	return os;
}

TreeWidget::TreeWidget(vtkPolyData *nodes, vtkPolyData *arcs)
	: tree_arcs(arcs), tree_nodes(nodes), zoom_amount(1.0)
{
	assert(tree_arcs);
	assert(tree_nodes);
	vtkDataSetAttributes *point_attribs = tree_arcs->GetAttributes(vtkDataSet::POINT);
	vtkPoints *points = tree_arcs->GetPoints();
	assert(points);

	vtkDataSetAttributes *cell_attribs = tree_arcs->GetAttributes(vtkDataSet::CELL);
	vtkCellArray *lines = tree_arcs->GetLines();
	assert(lines);
	// TODO: Build the branch structure which we can then draw. Ignore most
	// of VTK's crazy crap, we know we have lines and the layout of the points and lines
	// is as follows:
	// Points: [p_a, p_b, p_c, p_d, ...]
	// Lines: [p_a -> p_b, p_c -> p_d, ...]
	int idx = 0;
	vtkDataArray *pt_img_file = point_attribs->GetArray("ImageFile", idx);
	img_range.x = pt_img_file->GetRange()[0];
	img_range.y = pt_img_file->GetRange()[1];

	vtkDataArray *line_seg_id = cell_attribs->GetArray("SegmentationId", idx);
	std::cout << "There are " << line_seg_id->GetRange()[1] + 1 << " Unique segmentation ids\n";
	branches.resize(size_t(line_seg_id->GetRange()[1]) + 1, Branch());
	std::cout << "# of lines = " << lines->GetNumberOfCells() << "\n";

	Branch current_branch;
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
	}
	// Push on the last segmentation
	branches[current_branch.segmentation_id] = current_branch;

	// TODO: Go through all start/end points and find entering/exiting branches to build connectivity
	for (auto &b : branches) {
		// Find all branches entering this one (their end = our start)
		for (size_t i = 0; i < branches.size(); ++i){
			if (branches[i].end == b.start) {
				b.entering_branches.push_back(i);
			}
		}
		// Find all branches exiting this one (their start = our end)
		for (size_t i = 0; i < branches.size(); ++i){
			if (branches[i].start == b.end) {
				b.exiting_branches.push_back(i);
			}
		}
		//std::cout << b << "\n";
	}

	// Setup the display data
	build_ui_tree();
}
bool point_on_line(const glm::vec2 &start, const glm::vec2 &end, const glm::vec2 &point) {
	if (point.x < std::min(start.x, end.x) - 2 || point.x > std::max(start.x, end.x) + 2
			|| point.y < std::min(start.y, end.y) - 2 || point.y > std::max(start.y, end.y) + 2) {
		return false;
	}
	const glm::vec2 dir = end - start;
	const glm::vec2 n = glm::normalize(glm::vec2(dir.y, -dir.x));
	const float d = std::abs(glm::dot(n, point - start));
	// Measurements are in pixels
	return d < 2;
}
void TreeWidget::draw_ui() {
	if (!ImGui::Begin("Tree Widget")) {
		ImGui::End();
		return;
	}

	ImGui::Text("Contour/Split/Merge Tree");
	ImGui::Text("Left click to select branch, Ctrl-click to select multiple");
	if (ImGui::Button("Clear Selection")) {
		selected_segmentations.clear();
	}

	ImGui::SliderFloat("Zoom Tree", &zoom_amount, 1.0f, 35.f);

	const glm::vec2 canvas_size(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * zoom_amount);
	const glm::vec2 mouse_pos = glm::vec2(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
	ImGui::BeginChild("tree", canvas_size);

	const glm::vec2 canvas_pos(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
	const glm::vec2 view_scale(canvas_size.x - 16, -canvas_size.y / img_range.y);
	const glm::vec2 view_offset(canvas_pos.x + 8, canvas_pos.y + canvas_size.y);
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRect(canvas_pos, canvas_pos + canvas_size, ImColor(128, 128, 128));

	for (const auto &b : display_tree.branches) {
		const Branch &seg_branch = branches[b.first];

		const glm::vec2 start = display_tree.points[b.second.start] * view_scale + view_offset;
		const glm::vec2 end = display_tree.points[b.second.end] * view_scale + view_offset;
		if (point_on_line(start, end, mouse_pos)) {
			if (ImGui::IsMouseClicked(0)) {
				const bool was_selected = std::find(selected_segmentations.begin(), selected_segmentations.end(),
						b.first) != selected_segmentations.end();
				if (!ImGui::GetIO().KeyCtrl) {
					selected_segmentations.clear();
				}
				auto fnd = std::find(selected_segmentations.begin(), selected_segmentations.end(), b.first);
				if (fnd != selected_segmentations.end()) {
					selected_segmentations.erase(fnd);
				} else if (!was_selected) {
					selected_segmentations.push_back(b.first);
				}
			}

			std::string incident_list, exiting_list;
			for (const auto &x : seg_branch.entering_branches) {
				incident_list += std::to_string(x) + ", ";
			}
			for (const auto &x : seg_branch.exiting_branches) {
				exiting_list += std::to_string(x) + ", ";
			}
			ImGui::SetTooltip("Segmentation %lu, range [%.3f, %.3f]\n# Incident %lu - [%s]\n# Exiting %lu - [%s]",
					b.first, seg_branch.start_val, seg_branch.end_val, seg_branch.entering_branches.size(),
					incident_list.c_str(), seg_branch.exiting_branches.size(), exiting_list.c_str());
		}

		// Color format is AABBGGRR
		uint32_t color = 0xff1fefef;
		if (!selected_segmentations.empty()) {
			color = 0x4f1fefef;
			auto fnd = std::find(selected_segmentations.begin(), selected_segmentations.end(), b.first);
			if (fnd != selected_segmentations.end()) {
				color = 0xffffffff;
			} else {
				auto fnd = std::find_first_of(seg_branch.entering_branches.begin(), seg_branch.entering_branches.end(),
						selected_segmentations.begin(), selected_segmentations.end());
				if (fnd != seg_branch.entering_branches.end()) {
					color = 0xffff0000;
				}

				fnd = std::find_first_of(seg_branch.exiting_branches.begin(), seg_branch.exiting_branches.end(),
						selected_segmentations.begin(), selected_segmentations.end());
				if (fnd != seg_branch.exiting_branches.end()) {
					color = 0xff0000ff;
				}
			}
		}
		draw_list->AddLine(start, end, color);
	}
	ImGui::EndChild();

	ImGui::End();
}
const std::vector<uint32_t>& TreeWidget::get_selection() const {
	return selected_segmentations;
}

struct VecComparator {
	bool operator()(const glm::uvec3 &a, const glm::uvec3 &b) const {
		return a.x < b.x || (a.x == b.x && a.y < b.y)
			|| (a.x == b.x && a.y == b.y && a.z < b.z);
	}
};

void TreeWidget::build_ui_tree() {
	// Build a map of the 3D point to the graph point and use that to do the lookup for point ids
	std::map<glm::uvec3, size_t, VecComparator> point_map;
	std::set<float> fcn_vals;
	for (const auto &b : branches) {
		display_tree.branches[b.segmentation_id].segmentation = b.segmentation_id;

		// Look and see if a point for our start point has been made already by the branches
		// entering us
		size_t start_pt = display_tree.points.size();
#if 1
		// TODO: Fix the shared vertex finding
		for (const auto &x : b.entering_branches) {
			auto fnd = display_tree.branches.find(x);
			if (fnd != display_tree.branches.end()) {
				start_pt = fnd->second.end;
				break;
			}
		}
#endif
		if (start_pt == display_tree.points.size()) {
			display_tree.points.push_back(glm::vec2(0.f, b.start_val));
		}

		display_tree.branches[b.segmentation_id].start = start_pt;
#if 1
		for (const auto &x : b.entering_branches) {
			display_tree.branches[x].end = start_pt;
		}
#endif


		// Look and see if a point for our end point has been made already by the branches
		// exiting us
		size_t end_pt = display_tree.points.size();
#if 1
		for (const auto &x : b.exiting_branches) {
			auto fnd = display_tree.branches.find(x);
			if (fnd != display_tree.branches.end()) {
				end_pt = fnd->second.end;
				break;
			}
		}
#endif
		if (end_pt == display_tree.points.size()) {
			display_tree.points.push_back(glm::vec2(0.f, b.end_val));
		}

		display_tree.branches[b.segmentation_id].end = end_pt;
#if 1
		for (const auto &x : b.exiting_branches) {
			display_tree.branches[x].start = end_pt;
		}
#endif

		fcn_vals.insert(b.start_val);
		fcn_vals.insert(b.end_val);
	}

	std::cout << "# of nodes in tree = " << display_tree.points.size() << "\n";

	// Pass through all points at each function value and space them out nicely
	for (const auto &f : fcn_vals) {
		const size_t n = std::count_if(display_tree.points.begin(), display_tree.points.end(),
				[&](const glm::vec2 &v) { return v.y == f; });
		// This probably shouldn't happen, maybe precision issues?
		if (n == 0) {
			continue;
		}

		float i = 0;
		for (auto &p : display_tree.points) {
			if (p.y == f) {
				p.x = i / n;
				++i;
			}
		}
	}
}

