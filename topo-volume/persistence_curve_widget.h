#pragma once
#ifndef _PERSISTENCE_CURVE_WIDGET_H_
#define _PERSISTENCE_CURVE_WIDGET_H_

#include <map>
#include <vector>
#include <array>
#include <limits>
#include <cmath>
// include the vtk headers
#include <vtkSmartPointer.h>
#include <vtkPersistenceCurve.h>
// include the local headers
#include <glm/glm.hpp>
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
	std::vector<float> data;
	std::vector<float> axis;
	void Clear() { 
	    this->ID = 0;
	    this->last_frame = -1;
	    this->data_offset = 0;
	    this->data_count = 0;
	    this->scale_min = std::numeric_limits<int>::max();;
	    this->scale_max = std::numeric_limits<int>::min();;
	    this->data.clear();
	    this->axis.clear();
	}
	void AddValue(float var) {
	    this->AddValue(var, static_cast<float>(this->data_count+1)); 
	}
	void AddValue(glm::vec2 var) {
	    this->AddValue(var[1], var[0]); 
	}

	void AddValue(float var, float coord) {
	    this->data.push_back(var);
	    this->scale_min = std::min(this->scale_min, var);
	    this->scale_max = std::max(this->scale_max, var);
	    this->axis.push_back(coord);
	    ++(this->data_count);
	}
    };    
private:
    vtkSmartPointer<vtkPersistenceCurve> vtkcurve;
    std::array<CurveData, 3> curves; /* debug linear log */
    unsigned int debuglevel = 0;
    unsigned int numthread = 2;
    unsigned int plotxdim = 600;
    unsigned int plotydim = 100;
    unsigned int curve_idx = 2;
public:
    void debugdata() {
	for (int i = 0; i < 1000; ++i) { curves[0].AddValue((float)i); }
    }
    ~PersistenceCurveWidget() {}
    PersistenceCurveWidget() { this->debugdata(); }
    PersistenceCurveWidget(vtkDataSet* input, 
			   unsigned int debugLevel = 0, 
			   unsigned int numThread = 2)
    {
	this->debuglevel = debugLevel;
	this->numthread = numThread;
	this->compute(input);
    }
    PersistenceCurveWidget(const PersistenceCurveWidget&) = delete;
    PersistenceCurveWidget& operator=(const PersistenceCurveWidget&) = delete;
    /**
     * @brief plot curve
     */
    void draw(const char* label, CurveData& curve);
    void draw(const char* label = "Persistence Curve")
    {
	draw(label, curves[curve_idx]);
    }
    void draw(std::string scale, const char* label = "Persistence Curve") 
    { 
	if (scale.compare("debug") == 0) {
	    draw(label, curves[0]);
	} 
	else if (scale.compare("linear") == 0) {
	    draw(label, curves[1]);
	}
	else if (scale.compare("log") == 0) {
	    draw(label, curves[2]);
	}
	else {
	    std::cerr << "ERROR: plot type (" << scale << ") doesn't exist!" << std::endl;
	}
    }
    /**
     * @brief Call this to discard old/unused data
     */
    void flush(CurveData& curve) { curve.Clear(); }
    void flush() { 
	for (auto it = curves.begin(); it != curves.end(); ++it) {
	    (*it).Clear(); 
	}
    }

    /**
     * @brief compute curve
     */
    int compute(vtkDataSet*);
};

#endif//_PERSISTENCE_CURVE_WIDGET_H_
