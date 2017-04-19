#pragma once

#include <vector>
#include <vtkPolyData.h>

/* Displays and allows the user to interact with the contour/merge/split
 * tree produced by TTK for the dataset. The actively selected segmentations
 * can be queried for use in later filtering operations.
 */
class TreeWidget {
	vtkPolyData *tree_arcs, *tree_nodes;
	std::vector<uint32_t> selected_segmentations;

public:
	/* Construct the tree widget from the arc and node outputs
	 * from TTK's ContourForests VTK filter
	 */
	TreeWidget(vtkPolyData *nodes, vtkPolyData *arcs);
	void draw_ui();
	const std::vector<uint32_t>& get_selection() const;
};

