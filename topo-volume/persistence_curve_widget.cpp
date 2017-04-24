#include <algorithm>
#include <iostream>
#include <vtkIndent.h>
#include <vtkTable.h>
#include <vtkVariant.h>
#include "persistence_curve_widget.h"

PersistenceCurveWidget::PersistenceCurveWidget(vtkSmartPointer<vtkXMLImageDataReader> input) {
	diagram = vtkSmartPointer<vtkPersistenceDiagram>::New();
	diagram->SetInputConnection(input->GetOutputPort());

	{
		diagram->Update();
		vtkDataSetAttributes *fields = diagram->GetOutput()->GetAttributes(vtkDataSet::CELL);
		fields->PrintSelf(std::cout, vtkIndent(2));
		int idx = 0;
		vtkDataArray *data = fields->GetArray("PairIdentifier", idx);
		vtkDataArray *pers_data = fields->GetArray("Persistence", idx);
		std::cout << "Pair id range = [" << data->GetRange()[0] << ", " << data->GetRange()[1] << "]\n"
			"Persistence = [" << pers_data->GetRange()[0] << ", " << pers_data->GetRange()[1] << "]\n";
	}

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
void PersistenceCurveWidget::draw(const char* label, CurveData& curve)
{
    if (ImGui::Begin(label)) 
    {
        curve.ID = ImGui::GetID(label);

		// draw button
		ImGui::Checkbox("X log scale", &xlog);
		ImGui::SameLine();
		ImGui::Checkbox("y log scale", &ylog);
		std::string mode = "current scale: ";
		if (xlog && ylog) {
			curve_idx = 4;
		} 
		else if (xlog) {
			curve_idx = 3;
		} 
		else if (ylog) {
			curve_idx = 2;
		} 
		else {
			curve_idx = 1;
		}

		// draw two sliders
		bool persistence_updated = ImGui::SliderFloat("min persistence", &curve.threshold[0],
				//0, 498);
				//curve.data_min[0], curve.data_max[0]);
		persistence_updated |= ImGui::SliderFloat("max persistence", &curve.threshold[1],
				//0, 498);
				curve.data_min[0], curve.data_max[0]);
		// TODO WILL: These values are not in the right range. Why is the plot being drawn this way.
		if (persistence_updated) {
			std::cout << "It changed\n";
			if (curve.threshold[1] < curve.threshold[0]) {
				curve.threshold[1] = curve.threshold[0];
			}
			//persistent_pairs->ThresholdBetween(curve.threshold[0], curve.threshold[1]);
			persistent_pairs->ThresholdBetween(curve.threshold[0], curve.threshold[1]);
			simplification->Update();
		}

		// draw rect
		const int offset = 40;
		const glm::vec2 canvas_pos (ImGui::GetCursorScreenPos().x,    ImGui::GetCursorScreenPos().y + offset);
		const glm::vec2 canvas_size(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - offset);
		const glm::vec2 view_scale (canvas_size.x, -canvas_size.y);
		const glm::vec2 view_offset(canvas_pos.x, canvas_pos.y + canvas_size.y);

		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		draw_list->AddRect(canvas_pos, canvas_pos + canvas_size, ImColor(255, 0, 255));
		draw_list->PushClipRect(canvas_pos, canvas_pos + canvas_size);

		// draw curve
		const glm::vec2 scale = (curve.data_max - curve.data_min);
		// curve
		for (int i = 0; i < static_cast<int>(curve.data.size())-1; ++i) {
			const glm::vec2 ra = (curve.data[i]   - curve.data_min);
			const glm::vec2 rb = (curve.data[i+1] - curve.data_min);
			const glm::vec2 a(ra[0] / scale[0], ra[1] / scale[1]);
			const glm::vec2 b(rb[0] / scale[0], rb[1] / scale[1]);
			draw_list->AddLine(view_offset + view_scale * a, view_offset + view_scale * b, curve.color, 2.0f);
		}
		// threshold left
		{
			float v = (curve.threshold[0] - curve.data_min[0]) / scale[0];
			const glm::vec2 a(v, 0.0f);
			const glm::vec2 b(v, 1.0f);
			draw_list->AddLine(view_offset + view_scale * a, view_offset + view_scale * b, 0xff0000ff, 2.0f);
		}	    
		// threshold right
		{
			float v = (curve.threshold[1] - curve.data_min[0]) / scale[0];
			const glm::vec2 a(v, 0.0f);
			const glm::vec2 b(v, 1.0f);
			draw_list->AddLine(view_offset + view_scale * a, view_offset + view_scale * b, 0xff0000ff, 2.0f);
		}	    
		draw_list->PopClipRect();
	}
    ImGui::End();
}

int PersistenceCurveWidget::update()
{
    std::cout << "[computing curve] start" << std::endl;
	// Tree type mapping to persistence curve output index:
	// Join Tree: 0
	// Morse Smale Curve: 1
	// Split Tree: 2
	// Contour Tree: 3
	vtkTable* table = dynamic_cast<vtkTable*>(vtkcurve->GetOutputInformation(3)->Get(vtkDataObject::DATA_OBJECT()));
	table->PrintSelf(std::cout, vtkIndent(2));
    for(vtkIdType r = 0; r < table->GetNumberOfRows(); r++)
    {
	// x axis: persistence
	// y axis: number of pairs
	glm::vec2 curr(table->GetValue(r,0).ToFloat(), table->GetValue(r,1).ToFloat());
	// linear interpolate in between two values
	if (curr[0] > -0.1f && curr[1] > -0.1f) { // skip first entry
	    curves[1].AddValue(curr);	
	}
	// log interpolate in between two values
	if (curr[0] > 0.0f && curr[1] > 0.0f) { // skip first entry
	    curves[2].AddValue(curr[0], glm::log(curr[1]));
	    curves[3].AddValue(glm::log(curr[0]), curr[1]);
	    curves[4].AddValue(glm::log(curr));
	}
    }
    std::cout << "[computing curve] finish adding points" << std::endl;     
    std::cout << "[computing curve] Number of Rows: " << table->GetNumberOfRows() << std::endl;
    std::cout << "[computing curve] Number of Columns: " << table->GetNumberOfColumns() << std::endl;
    return 0;
}
