#include <algorithm>
#include <thread>
#include <iostream>
#include <glm/ext.hpp>
#include <vtkIndent.h>
#include <vtkTable.h>
#include <vtkVariant.h>
#include <vtkUnstructuredGrid.h>
#include "persistence_curve_widget.h"

PersistenceCurveWidget::PersistenceCurveWidget(vtkSmartPointer<vtkXMLImageDataReader> input)
    : tree_type(ttk::TreeType::Split)
{
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
    // Start at a persistence above one so we filter out some junk
    threshold_range[0] = 4.f;
    persistent_pairs->ThresholdBetween(threshold_range[0], 999999);

    // Simplifying the input data to remove non-persistent pairs
    simplification = vtkSmartPointer<vtkTopologicalSimplification>::New();
    simplification->SetUseAllCores(true);
    simplification->SetThreadNumber(std::thread::hardware_concurrency());
    simplification->SetInputConnection(0, input->GetOutputPort());
    simplification->SetInputConnection(1, persistent_pairs->GetOutputPort());

    // We always show the full curve, without simplfication for the
    // selected tree type
    vtkcurve = vtkSmartPointer<vtkPersistenceCurve>::New();
    vtkcurve->SetInputConnection(input->GetOutputPort());
    vtkcurve->SetComputeSaddleConnectors(false);
    vtkcurve->SetUseAllCores(true);
    vtkcurve->SetThreadNumber(std::thread::hardware_concurrency());
    vtkcurve->Update();
    update_persistence_curve();
    update_persistence_diagram();
}
vtkTopologicalSimplification* PersistenceCurveWidget::get_simplification() const {
    return simplification.Get();
}
void PersistenceCurveWidget::draw_ui() {
    if (ImGui::Begin("Persistence Plots")) 
    {
	// we need to tell how large each plot is, so its better to plot sliders here
	ImGui::Text("Persistence Range [%.2f, %.2f]", persistence_range.x, persistence_range.y);
	ImGui::SliderFloat("Threshold", &threshold_range[0], persistence_range.x,
			   persistence_range.y, "%.3f", glm::e<float>());
	// Keep values in range
	threshold_range[0] = glm::max(threshold_range[0], persistence_range.x);

	// If we changed our selection update the display in the other widgets
	if (ImGui::Button("Apply")) {
	    persistent_pairs->ThresholdBetween(threshold_range[0], threshold_range[1]);
	    simplification->Update();
	}

	// draw plots
	float fullheight = ImGui::GetContentRegionAvail().y;
	draw_persistence_curve(fullheight * 0.5);
	draw_persistence_diagram(fullheight * 0.5);
    }
    ImGui::End();
}
void PersistenceCurveWidget::draw_persistence_curve(float ysize) {
    // Draw the persistence plot
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, glm::vec2(1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, glm::vec2(0));
    ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(60, 60, 70, 200));
    ImGui::BeginChild("pers_curve", glm::vec2(0, ysize), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

    const glm::vec2 canvas_size(ImGui::GetContentRegionAvail());
    const glm::vec2 offset = glm::vec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + canvas_size.y);
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

    // Draw threshold line
    {
	const float v = std::log(threshold_range.x) * view_scale.x + offset.x;
	const glm::vec2 a = glm::vec2(v, offset.y);
	const glm::vec2 b = glm::vec2(v, offset.y - canvas_size.y);
	draw_list->AddLine(a, b, ImColor(0.8f, 0.8f, 0.2f, 1.f), 2.f);
    }

    draw_list->PopClipRect();

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
void PersistenceCurveWidget::draw_persistence_diagram(float ysize) {
    ImGui::Text("Persistence Diagram...");

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, glm::vec2(1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, glm::vec2(0));
    ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(60, 60, 70, 200));
    ImGui::BeginChild("pers_diag", glm::vec2(0, ysize - 20.0f /* text y-offset */), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
	
    // TODO: Persistence Diagram
    const glm::vec2 canvas_size(ImGui::GetContentRegionAvail());
    const glm::vec2 offset = glm::vec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + canvas_size.y);

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(ImGui::GetCursorScreenPos(), glm::vec2(ImGui::GetCursorScreenPos()) + canvas_size);

    draw_list->PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
void PersistenceCurveWidget::set_tree_type(const ttk::TreeType &type) {
    if (tree_type != type) {
	tree_type = type;
	update_persistence_curve();
    }
}
void PersistenceCurveWidget::update_persistence_curve() {
    // Tree type mapping to persistence curve output index:
    // Join Tree: 0
    // Morse Smale Curve: 1
    // Split Tree: 2
    // Contour Tree: 3
    int tree = 0;
    switch (tree_type) {
    case ttk::TreeType::Contour: tree = 3; break;
    case ttk::TreeType::Split: tree = 2; break;
    case ttk::TreeType::Join: tree = 0; break;
    default: break;
    }
    vtkTable* table = dynamic_cast<vtkTable*>(vtkcurve->GetOutputInformation(tree)->Get(vtkDataObject::DATA_OBJECT()));
    vtkDataArray *persistence_col = dynamic_cast<vtkDataArray*>(table->GetColumn(0));
    vtkDataArray *npairs_col = dynamic_cast<vtkDataArray*>(table->GetColumn(1));

    assert(persistence_col->GetSize() == npairs_col->GetSize());
    curve_points.clear();
    curve_points.reserve(persistence_col->GetSize());

    npairs_range = glm::vec2(std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
    persistence_range = glm::vec2(1.f, -std::numeric_limits<float>::infinity());
    std::map<float, size_t> pers_count;
    for (vtkIdType i = 0; i < persistence_col->GetSize(); ++i) {
	// It seems that often there are many entries with the same persistence value, count these up instead of making
	// a bunch of dot lines
	const float p = *persistence_col->GetTuple(i);
	if (p > 0.f) {
	    persistence_range.y = std::max(persistence_range.y, p);
	    const float n = *npairs_col->GetTuple(i);
	    npairs_range.x = std::min(npairs_range.x, static_cast<float>(n));
	    npairs_range.y = std::max(npairs_range.y, static_cast<float>(n));
	    curve_points.push_back(glm::vec2(p, n));
	}
    }
    threshold_range[1] = persistence_range.y;
    persistent_pairs->ThresholdBetween(threshold_range[0], threshold_range[1]);
    simplification->Update();
}
void PersistenceCurveWidget::update_persistence_diagram() {
    vtkUnstructuredGrid* diagram_cells = vtkUnstructuredGrid::SafeDownCast(diagram->GetOutput());

    // diagram_cells->PrintSelf(std::cout, vtkIndent(0));

    auto array_PairIdentifier = diagram_cells->GetCellData()->GetArray(0);
    auto array_PairType       = diagram_cells->GetCellData()->GetArray(1);
    auto array_Persistence    = diagram_cells->GetCellData()->GetArray(2);

    for (vtkIdType i = 0; i < static_cast<vtkIdType>(array_PairType->GetNumberOfCells()); ++i) {
	// vtkCell* cell = diagram_cells->GetCell(i)GetTuple;
    }
}
