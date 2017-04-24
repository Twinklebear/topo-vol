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
	if (ImGui::Button("Change Mode")) {
	    curve_idx = 1 + (curve_idx + 1) % (curves.size() - 1);
	}
	// draw curve
	int current_frame = ImGui::GetFrameCount();
	float scale_min, scale_max;
	if (curve.scale_min > curve.scale_max) {
	    scale_min = FLT_MAX;
	    scale_max = FLT_MAX;
	}
	else {
	    scale_min = curve.scale_min;
	    scale_max = curve.scale_max;
	}
	if (curve.last_frame != current_frame)
	{
	    std::string mode;
	    switch (curve_idx) {
	    case(0):
		mode = "debug"; break;
	    case(1):
		mode = "linear curve"; break;
	    case(2):
		mode = "log curve"; break;				    
	    }
	    ImGui::PlotLines("curve", curve.data.data(), 
			     curve.data_count, 
			     curve.data_offset, 
			     NULL, 
			     scale_min, 
			     scale_max, 
			     ImVec2(plotxdim, plotydim));
	    curve.last_frame = current_frame;
	}
    }
    ImGui::End();
}

int PersistenceCurveWidget::compute(vtkDataSet* input)
{
    vtkcurve = vtkSmartPointer<vtkPersistenceCurve>::New();
    vtkcurve->SetInputData(input);
    vtkcurve->SetComputeSaddleConnectors(false);
    vtkcurve->SetThreadNumber(numthread);
    vtkcurve->SetdebugLevel_(debuglevel);
    vtkcurve->Update();

    std::cout << "[computing curve] start" << std::endl;
    // colume #0 number of pairs
    // colume #1 persistence
    vtkTable* table = vtkcurve->getResult(2);  // all pairs
    glm::vec2 last(-1.0f, -1.0f);
    glm::vec2 curr(-1.0f, -1.0f);
    // axes limits [min, max]
    // glm::vec2 xlim(std::numeric_limits<int>::max(), std::numeric_limits<int>::min());
    // glm::vec2 ylim(std::numeric_limits<int>::max(), std::numeric_limits<int>::min());
    // // get limit
    // for(vtkIdType r = 0; r < table->GetNumberOfRows(); r++)
    // {
    // 	// x axis: persistence
    // 	// y axis: number of pairs
    // 	curr = glm::vec2(table->GetValue (r,1).ToFloat(), table->GetValue (r,0).ToFloat());
    // 	if (curr[0] <= 0.0f || curr[1] <= 0.0f){ continue; }
    // 	xlim[0] = std::min(xlim[0], curr[0]);
    // 	xlim[1] = std::max(xlim[1], curr[0]);
    // 	ylim[0] = std::min(ylim[0], curr[1]);
    // 	ylim[1] = std::max(ylim[1], curr[1]);
    // }
    // get samples
    glm::vec2 diff;
    float sample_step = 1.0f / 10.0f;
    for(vtkIdType r = 0; r < table->GetNumberOfRows(); r++)
    {
	// x axis: persistence
	// y axis: number of pairs
	curr = glm::vec2(table->GetValue(r,0).ToFloat(), table->GetValue(r,1).ToFloat());
	// linear interpolate in between two values
	if (curr[0] != last[0] && last[0] > -0.1f && last[1] > -0.1f) { // skip first entry
	    diff = curr - last;
	    for (float x = 0.0f; x <= diff[0]; x += sample_step) {
		glm::vec2 val = glm::mix(last, curr, x / diff[0]);
		curves[1].AddValue(val);
	    }
	}
	// log interpolate in between two values
	if (curr[0] != last[0] && last[0] > 0.0f && last[1] > 0.0f) { // skip first entry
	    diff = glm::log(curr) - glm::log(last);
	    for (float x = 0.0f; x <= diff[0]; x += sample_step) {
		glm::vec2 val = glm::mix(last, curr, x / diff[0]);
		curves[2].AddValue(glm::log(val));
		std::cout << "[computing curve] point " << glm::log(val)[0] << " " << glm::log(val)[1] << std::endl;     
	    }
	}
	last = curr;
    }
    std::cout << "[computing curve] finish adding points" << std::endl;     
    std::cout << "[computing curve] Number of Rows: " << table->GetNumberOfRows() << std::endl;
    std::cout << "[computing curve] Number of Columns: " << table->GetNumberOfColumns() << std::endl;
    return 0;
}
