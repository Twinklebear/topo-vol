#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <ostream>
#include <vector>
#include <vtkPolyData.h>
#include <vtkCommand.h>
#include <vtkSmartPointer.h>
#include <ttkFTMTree.h>
#include <ttkTopologicalSimplification.h>

// A branch in the tree, representing a specific segmentation
// id of the data
struct Branch {
	int segmentation_id;
	// Start/end points of the branch in voxels
	glm::uvec3 start, end;
	// Values of the volume at the branch's start/end points
	float start_val, end_val;
	// Start and end nodes
	int64_t start_node, end_node;
};
std::ostream& operator<<(std::ostream &os, const Branch &b);

struct TreeNode {
	size_t node_id;
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

/* Displays and allows the user to interact with the contour/merge/split
 * tree produced by TTK for the dataset. The actively selected segmentations
 * can be queried for use in later filtering operations.
 */
class TreeWidget : public vtkCommand {
	vtkSmartPointer<ttkFTMTree> contour_forest;
	int tree_type;
	vtkUnstructuredGrid *tree_nodes;
	vtkUnstructuredGrid *tree_arcs;
	std::vector<uint32_t> selected_segmentations;
	std::vector<Branch> branches;
	std::vector<TreeNode> nodes;
	float zoom_amount;
	glm::vec2 scrolling;

public:
	/* Construct the tree widget from the arc and node outputs
	 * from TTK's FTMTree VTK filter. Will watch the simplification
	 * for changes and re-update the contour forest accordingly
	 */
	TreeWidget(vtkSmartPointer<ttkFTMTree> contour_forest,
			ttkTopologicalSimplification *simplification);
	void draw_ui();
	const std::vector<uint32_t>& get_selection() const;
	// Get the current tree type
        ttk::ftm::TreeType get_tree_type() const; 
	void Execute(vtkObject *caller, unsigned long event_id, void *call_data) override;

private:
	// Build the connectivity of the tree from the information TTK gives us
	void build_tree();
};

