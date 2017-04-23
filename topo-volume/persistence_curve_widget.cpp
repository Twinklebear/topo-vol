#include <algorithm>
#include <iostream>
#include "persistence_curve_widget.h"

static PersistenceCurveWidget::CurveMap curve_map;

PersistenceCurveWidget::PersistenceCurveWidget() {}

PersistenceCurveWidget::~PersistenceCurveWidget() {}

void PersistenceCurveWidget::draw(const char* label, float value, float scale_min, float scale_max, size_t buffer_size)
{
    if (buffer_size == 0) { buffer_size = 120; }

    ImGui::PushID(label);
    ImGuiID id = ImGui::GetID("");

    // Lookup O(log N)
    CurveData& pvd = curve_map[id];

    // Setup
    if (pvd.data.capacity() != buffer_size)
    {
	pvd.data.resize(buffer_size);
	memset(&pvd.data[0], 0, sizeof(float) * buffer_size);
	pvd.data_insert_idx = 0;
	pvd.last_frame = -1;
    }

    // Insert (avoid unnecessary modulo operator)
    if (pvd.data_insert_idx == buffer_size)
	pvd.data_insert_idx = 0;
    int display_idx = pvd.data_insert_idx;
    if (value != FLT_MAX)
	pvd.data[pvd.data_insert_idx++] = value;

    // Draw
    int current_frame = ImGui::GetFrameCount();
    if (pvd.last_frame != current_frame)
    {
	ImGui::PlotLines("##plot", &pvd.data[0], buffer_size, pvd.data_insert_idx, NULL, scale_min, scale_max, ImVec2(0, 40));
	ImGui::SameLine();
	ImGui::Text("%s\n%-3.4f", label, pvd.data[display_idx]); // Display last value in buffer
	pvd.last_frame = current_frame;
    }
    
    ImGui::PopID();
}

void PersistenceCurveWidget::flush() 
{
    int current_frame = ImGui::GetFrameCount();
    for (CurveMap::iterator it = curve_map.begin(); it != curve_map.end(); )
    {
	CurveData& pvd = it->second;
	if (pvd.last_frame < current_frame - std::max(400,(int)pvd.data.size()))
	    it = curve_map.erase(it);
	else
	    ++it;
    }
}

