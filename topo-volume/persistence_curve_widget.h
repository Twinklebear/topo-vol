#pragma once
#ifndef _PERSISTENCE_CURVE_WIDGET_H_
#define _PERSISTENCE_CURVE_WIDGET_H_

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
	ImGuiID        ID = 0;
	int            last_frame  = -1;
	int            data_offset = 0;
	int            data_count  = 0;
	float          scale_min = std::numeric_limits<int>::max();
	float          scale_max = std::numeric_limits<int>::min();
	std::vector<float>  data;
	void Clear() { 
	    this->ID = 0;
	    this->last_frame = -1;
	    this->data_offset = 0;
	    this->data_count = 0;
	    this->scale_min = std::numeric_limits<int>::max();;
	    this->scale_max = std::numeric_limits<int>::min();;
	    this->data.clear();
	}
	void AddValue(float var) {
	    this->data.push_back(var);
	    this->scale_min = std::min(this->scale_min, var);
	    this->scale_max = std::max(this->scale_max, var);
	    ++this->data_count;
	}
    };    
private:
    CurveData default_curve;
public:
    PersistenceCurveWidget() {
	for (int i = 0; i < 10; ++i)
	    default_curve.AddValue((float)i);
    }
    ~PersistenceCurveWidget() {}
    PersistenceCurveWidget(const PersistenceCurveWidget&) = delete;
    PersistenceCurveWidget& operator=(const PersistenceCurveWidget&) = delete;
    /**
     * @brief plot curve
     */
    void draw(const char* label, CurveData& curve);
    void draw(const char* label = "Persistence Curve") { draw(label, default_curve); }
    /**
     * @brief Call this to discard old/unused data
     */
    void flush(CurveData& curve) { curve.Clear(); }
    void flush() { default_curve.Clear(); }
};

#endif//_PERSISTENCE_CURVE_WIDGET_H_
