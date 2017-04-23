#pragma once

#include <map>
#include <vector>
#include "imgui-1.49/imgui.h"

/**
 * @brief render persistence curve
 * @reference https://github.com/ocornut/imgui/wiki/plot_var_example
 */
class PersistenceCurveWidget {
public:
    struct CurveData
    {
	ImGuiID        ID;
	int            data_insert_idx;
	int            last_frame;
	std::vector<float>  data;
        CurveData() : ID(0), data_insert_idx(0), last_frame(-1) {}
    };
    typedef std::map<ImGuiID, CurveData> CurveMap;
public:
	PersistenceCurveWidget();
	~PersistenceCurveWidget();
	PersistenceCurveWidget(const PersistenceCurveWidget&) = delete;
        PersistenceCurveWidget& operator=(const PersistenceCurveWidget&) = delete;
	/**
	 * @brief Plot value over time
	 * Pass FLT_MAX value to draw without adding a new value
	 */
	void draw(const char* label, float value, 
		  float scale_min = FLT_MAX, 
		  float scale_max = FLT_MAX, 
		  size_t buffer_size = 120);
	/**
	 * @brief Call this periodically to discard old/unused data
	 */
	void flush();
};

