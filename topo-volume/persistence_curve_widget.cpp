#include <algorithm>
#include <iostream>
#include <glm/ext.hpp>
#include <vtkIndent.h>
#include <vtkTable.h>
#include <vtkVariant.h>
#include "persistence_curve_widget.h"

PersistenceCurveWidget::PersistenceCurveWidget(vtkSmartPointer<vtkXMLImageDataReader> input) {
	diagram = vtkSmartPointer<vtkPersistenceDiagram>::New();
	diagram->SetInputConnection(input->GetOutputPort());

	// Compute critical points
	critical_pairs = vtkSmartPointer<vtkThreshold>::New();
	critical_pairs->SetInputConnection(diagram->GetOutputPort());
	critical_pairs->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "PairIdentifier");
	critical_pairs->ThresholdBetween(-0.1, 999999);

	// Select the most persistent pairs
	persistent_pairs = vtkSmartPointer<vtkThreshold>::New();
	persistent_pairs->SetInputConnection(critical_pairs->GetOutputPort());
	persistent_pairs->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "Persistence");
	persistent_pairs->ThresholdBetween(0, 999999);

	// Simplifying the input data to remove non-persistent pairs
	simplification = vtkSmartPointer<vtkTopologicalSimplification>::New();
	simplification->SetInputConnection(0, input->GetOutputPort());
	simplification->SetInputConnection(1, persistent_pairs->GetOutputPort());

	// We always show the full curve, without simplfication for the
	// selected tree type
    vtkcurve = vtkSmartPointer<vtkPersistenceCurve>::New();
    vtkcurve->SetInputConnection(input->GetOutputPort());
    vtkcurve->SetComputeSaddleConnectors(false);
    vtkcurve->SetUseAllCores(true);
    vtkcurve->Update();
	update();
}
vtkTopologicalSimplification* PersistenceCurveWidget::get_simplification() const {
	return simplification.Get();
}
void PersistenceCurveWidget::draw_ui() {
	if (!ImGui::Begin("Persistence Curve")) {
		ImGui::End();
		return;
	}

	// TODO: Switch to an apply button
	const bool persistence_updated = ImGui::SliderFloat2("Persistence Range", &threshold_range[0],
			persistence_range.x, persistence_range.y);
	// If we changed our selection update the display in the other widgets
	if (persistence_updated) {
		// Keep values in range
		threshold_range.x = glm::max(threshold_range.x, persistence_range.x);
		threshold_range.y = glm::max(threshold_range.x, threshold_range.y);
		threshold_range.y = glm::min(threshold_range.y, persistence_range.y);

		persistent_pairs->ThresholdBetween(threshold_range[0], threshold_range[1]);
		simplification->Update();
	}

	// Draw the persistence plot
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, glm::vec2(1));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, glm::vec2(0));
	ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(60, 60, 70, 200));
	ImGui::BeginChild("tree_region", glm::vec2(0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

	const glm::vec2 canvas_size(ImGui::GetContentRegionAvail());
	const glm::vec2 offset = glm::vec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + canvas_size.y);
	const glm::vec2 mouse_pos = glm::vec2(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
	const glm::vec2 axis_range = glm::log(glm::vec2(persistence_range.y, npairs_range.y));
	const glm::vec2 view_scale = glm::vec2(canvas_size.x / axis_range.x, -canvas_size.y / axis_range.y);

	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	draw_list->PushClipRect(ImGui::GetCursorScreenPos(), glm::vec2(ImGui::GetCursorScreenPos()) + canvas_size);

	// Draw curve
	for (size_t i = 0; !curve_points.empty() && i < curve_points.size() - 1; ++i) {
		const glm::vec2 a = glm::log(curve_points[i]);
		const glm::vec2 b = glm::log(curve_points[i + 1]);
		draw_list->AddLine(offset + view_scale * a, offset + view_scale * b, ImColor(255, 255, 255), 2.0f);
	}

	// Draw threshold lines
	for (size_t i = 0; i < 2; ++i) {
		const float v = std::log(threshold_range[i]) * view_scale.x + offset.x;
		const glm::vec2 a = glm::vec2(v, offset.y);
		const glm::vec2 b = glm::vec2(v, offset.y - canvas_size.y);
		draw_list->AddLine(a, b, ImColor(0.8f, 0.8f, 0.2f, 1.f), 2.f);
	}	    
	draw_list->PopClipRect();

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	ImGui::End();
}
void PersistenceCurveWidget::update() {
	// Tree type mapping to persistence curve output index:
	// Join Tree: 0
	// Morse Smale Curve: 1
	// Split Tree: 2
	// Contour Tree: 3
	vtkTable* table = dynamic_cast<vtkTable*>(vtkcurve->GetOutputInformation(0)->Get(vtkDataObject::DATA_OBJECT()));
	table->PrintSelf(std::cout, vtkIndent(2));
	vtkDataArray *persistence_col = dynamic_cast<vtkDataArray*>(table->GetColumn(0));
	vtkDataArray *npairs_col = dynamic_cast<vtkDataArray*>(table->GetColumn(1));

	persistence_range[0] = 1.f;
	persistence_range[1] = persistence_col->GetRange()[1];
	threshold_range = persistence_range;

	assert(persistence_col->GetSize() == npairs_col->GetSize());
	curve_points.clear();
	curve_points.reserve(persistence_col->GetSize());

	std::map<float, size_t> pers_count;
	for (vtkIdType i = 0; i < persistence_col->GetSize(); ++i) {
		// It seems that often there are many entries with the same persistence value, count these up instead of making
		// a bunch of dot lines
		const float p = *persistence_col->GetTuple(i);
		if (p > 0.f) {
			pers_count[p] += *npairs_col->GetTuple(i);
		}
	}

	for (const auto &v : pers_count) {
		npairs_range.x = std::min(npairs_range.x, static_cast<float>(v.second));
		npairs_range.y = std::max(npairs_range.y, static_cast<float>(v.second));
		curve_points.push_back(glm::vec2(v.first, v.second));
	}
}

