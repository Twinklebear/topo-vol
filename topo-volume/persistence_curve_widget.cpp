#include <algorithm>
#include <iostream>
#include "persistence_curve_widget.h"

void PersistenceCurveWidget::draw
(const char* label, CurveData& curve)
{
    if (ImGui::Begin(label)) 
    {
        curve.ID = ImGui::GetID(label);

	// draw
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
	    ImGui::PlotLines("curve", curve.data.data(), 
			     curve.data_count, 
			     curve.data_offset, 
			     NULL, 
			     scale_min, 
			     scale_max, 
			     ImVec2(40, 40));
	    curve.last_frame = current_frame;
	}
    }
    ImGui::End();
}



