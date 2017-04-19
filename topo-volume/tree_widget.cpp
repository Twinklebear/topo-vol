#include <cassert>
#include <iostream>
#include "imgui-1.49/imgui.h"
#include "tree_widget.h"

TreeWidget::TreeWidget(vtkPolyData *nodes, vtkPolyData *arcs)
	: tree_arcs(arcs), tree_nodes(nodes)
{
	assert(tree_arcs);
	assert(tree_nodes);
	std::cout << "Arcs:\n";
	tree_arcs->PrintSelf(std::cout, vtkIndent(0));
	std::cout << "Nodes:\n";
	tree_nodes->PrintSelf(std::cout, vtkIndent(0));
}
void TreeWidget::draw_ui() {
	if (!ImGui::Begin("Tree Widget")) {
		ImGui::End();
		return;
	}

	ImGui::End();
}
const std::vector<uint32_t>& TreeWidget::get_selection() const { 
	return selected_segmentations;
}

