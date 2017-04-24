#include <algorithm>
#include <iostream>
#include <vtkIndent.h>
#include <vtkTable.h>
#include <vtkVariant.h>
#include "persistence_curve_widget.h"

void PersistenceCurveWidget::draw
(const char* label, CurveData& curve)
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
	ImGui::SliderFloat("min persistence", &curve.threshold[0], curve.data_min[0], curve.data_max[0]);
	ImGui::SliderFloat("max persistence", &curve.threshold[1], curve.data_min[0], curve.data_max[0]);
	
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
	int current_frame = ImGui::GetFrameCount();
	if (curve.last_frame != current_frame)
	{
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
	    curve.last_frame = current_frame;
	}
	draw_list->PopClipRect();
    }
    ImGui::End();
}

int PersistenceCurveWidget::compute(vtkDataSet* input)
{
    std::cout << "[PersistenceCurve] start" << std::endl;
    vtkcurve = vtkSmartPointer<vtkPersistenceCurve>::New();
    vtkcurve->SetInputData(input);
    vtkcurve->SetComputeSaddleConnectors(false);
    vtkcurve->SetUseAllCores(true);
    vtkcurve->SetdebugLevel_(debuglevel);
    vtkcurve->Update();

    std::cout << "[computing curve] start" << std::endl;
	// Tree type mapping to persistence curve output index:
	// Join Tree: 0
	// Morse Smale Curve: 1
	// Split Tree: 2
	// Contour Tree: 3
	vtkTable* table = dynamic_cast<vtkTable*>(vtkcurve->GetOutputInformation(3)->Get(vtkDataObject::DATA_OBJECT()));
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
