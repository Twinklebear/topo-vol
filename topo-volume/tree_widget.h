#pragma once

#include <unordered_map>
#include <ostream>
#include <vector>
#include <vtkPolyData.h>

// A branch in the tree, representing a specific segmentation
// id of the data
struct Branch {
	size_t segmentation_id;
	// Start/end points of the branch in voxels
	glm::uvec3 start, end;
	// Values of the volume at the branch's start/end points
	float start_val, end_val;
	// Branches entering and exiting this branches start/end points respectively
	std::vector<size_t> entering_branches, exiting_branches;
	// Start and end nodes
	size_t start_node, end_node;
};
std::ostream& operator<<(std::ostream &os, const Branch &b);

struct TreeNode {
	glm::uvec3 pos;
	float value;
	size_t type;
	std::vector<size_t> entering_branches, exiting_branches;

	// Info for displaying the corresponding ui
	glm::vec2 ui_pos, ui_size;

	glm::vec2 get_input_slot_pos(const size_t slot) const;
	glm::vec2 get_output_slot_pos(const size_t slot) const;
};
std::ostream& operator<<(std::ostream &os, const TreeNode &n);

struct DisplayBranch {
	size_t start, end, segmentation;
};

// The tree to display in the widget
struct DisplayTree {
	std::unordered_map<size_t, DisplayBranch> branches;
	std::vector<glm::vec2> points;
};

/* Displays and allows the user to interact with the contour/merge/split
 * tree produced by TTK for the dataset. The actively selected segmentations
 * can be queried for use in later filtering operations.
 */
class TreeWidget {
	vtkPolyData *tree_arcs, *tree_nodes;
	std::vector<uint32_t> selected_segmentations;
	std::vector<Branch> branches;
	std::vector<TreeNode> nodes;
	DisplayTree display_tree;
	glm::vec2 img_range;
	float zoom_amount;
	glm::vec2 scrolling;

public:
	/* Construct the tree widget from the arc and node outputs
	 * from TTK's ContourForests VTK filter
	 */
	TreeWidget(vtkPolyData *nodes, vtkPolyData *arcs);
	void draw_ui();
	const std::vector<uint32_t>& get_selection() const;

private:
	//void build_ui_tree();
};

