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
};
std::ostream& operator<<(std::ostream &os, const Branch &b);

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
	DisplayTree display_tree;
	glm::vec2 img_range;
	float zoom_amount;

public:
	/* Construct the tree widget from the arc and node outputs
	 * from TTK's ContourForests VTK filter
	 */
	TreeWidget(vtkPolyData *nodes, vtkPolyData *arcs);
	void draw_ui();
	const std::vector<uint32_t>& get_selection() const;

private:
	void build_ui_tree();
};

