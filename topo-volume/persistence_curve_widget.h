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
	int            color = 0xffffffff;
	int            last_frame  = -1;
	int            data_offset = 0;
	glm::vec2      threshold = glm::vec2(-1.0f, -1.0f);
	glm::vec2      data_min = glm::vec2(std::numeric_limits<int>::max());
	glm::vec2      data_max = glm::vec2(std::numeric_limits<int>::min());
	std::vector<glm::vec2> data;
	void Clear() { 
	    this->ID = 0;
	    this->last_frame = -1;
	    this->data_offset = 0;
	    this->data_min = glm::vec2(std::numeric_limits<int>::max());
	    this->data_max = glm::vec2(std::numeric_limits<int>::min());
	    this->data.clear();
	}
	void AddValue(float coord, float var) {
	    this->AddValue(glm::vec2(coord, var)); 
	}
	void AddValue(glm::vec2 var) {
	    this->data.push_back(var);
	    this->data_min = glm::min(this->data_min, var);
	    this->data_max = glm::max(this->data_max, var);
	}
    };    
private:
    vtkSmartPointer<vtkPersistenceCurve> vtkcurve;
    std::array<CurveData, 5> curves; /* debug linear xlinear-ylog xlog-ylinear xlog-ylog */
    unsigned int debuglevel = 0;
    unsigned int plotxdim = 600;
    unsigned int plotydim = 100;
    unsigned int curve_idx = 2;
    bool xlog = false, ylog = false;
public:
    void debugdata() {
	for (int i = 0; i < 1000; ++i) { curves[0].AddValue((float)i, (float)i); }
    }
    ~PersistenceCurveWidget() {}
    PersistenceCurveWidget() { this->debugdata(); }
    PersistenceCurveWidget(vtkDataSet* input, unsigned int debugLevel = 0)
    {
	this->debuglevel = debugLevel;
	this->compute(input);
    }
    PersistenceCurveWidget(const PersistenceCurveWidget&) = delete;
    PersistenceCurveWidget& operator=(const PersistenceCurveWidget&) = delete;
    /**
     * @brief get threshold
     */
    glm::vec2 threshold() { return curves[curve_idx].threshold; }
    /**
     * @brief plot curve
     */
    void draw(const char* label, CurveData& curve);
    void draw(const char* label = "Persistence Curve")
    {
	draw(label, curves[curve_idx]);
    }
    void draw(std::string drawtype, const char* label = "Persistence Curve") 
    { 
	if (drawtype.compare("debug") == 0) {
	    draw(label, curves[0]);
	} 
	else if (drawtype.compare("linear") == 0) {
	    draw(label, curves[1]);
	}
	else if (drawtype.compare("log") == 0) {
	    draw(label, curves[4]);
	}
	else {
	    std::cerr << "ERROR: plot type (" << drawtype << ") doesn't exist!" << std::endl;
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
