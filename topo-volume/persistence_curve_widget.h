#pragma once

#include <map>
#include <vector>
#include <array>
#include <limits>
#include <cmath>
// include the vtk headers
#include <vtkSmartPointer.h>
#include <vtkXMLImageDataReader.h>
#include <vtkThreshold.h>
#include <ttkPersistenceCurve.h>
#include <ttkPersistenceDiagram.h>
#include <ttkTopologicalSimplification.h>
// include the local headers
#include <glm/glm.hpp>
#include "imgui-1.49/imgui.h"

/**
 * @brief render persistence curve
 * @reference https://github.com/ocornut/imgui/wiki/plot_var_example
 */
class PersistenceCurveWidget {
public:
    struct Line {
	Line() {}
    Line(double start[2], double end[2], double line_type, double node_start_type, double node_end_type)
	: ps(glm::make_vec2(start)), pe(glm::make_vec2(end)), tline{line_type}, tnode{node_start_type, node_end_type} {}
	glm::vec2 ps, pe;
	double tline;
	double tnode[2];
    };
private:
    vtkSmartPointer<ttkPersistenceDiagram> diagram;
    vtkSmartPointer<vtkThreshold> critical_pairs, persistent_pairs;
    vtkSmartPointer<ttkTopologicalSimplification> simplification;
    vtkSmartPointer<ttkPersistenceCurve> vtkcurve;
    ttk::ftm::TreeType tree_type;

    // Data for the ui display
    std::vector<glm::vec2> curve_points;
    std::vector<Line> diagram_lines;
    glm::vec2 persistence_range, npairs_range, threshold_range;
    
    // debug level
    unsigned int debuglevel = 0;
public:
    /**
     * @brief Setup the persistence curve display for the passed volume data. The
     * topological simplification selected by the user can then be gotten
     * via `get_simplification`
     */
    PersistenceCurveWidget(vtkImageData *data, unsigned int debug = 0);
    // Get the topological simplification resulting from the user's selection
    ttkTopologicalSimplification* get_simplification() const;
    /**
     * @brief plot curve
     */
    void draw_ui();
    // Set the tree type we should be showing the persistence curve for
    void set_tree_type(const ttk::ftm::TreeType &type);

private:
    void draw_persistence_curve(float ysize = 0.0f /* zero means using the whole area */);
    void draw_persistence_diagram(float ysize = 0.0f);
    /**
     * @brief compute curve
     */
    void update_persistence_curve();
    void update_persistence_diagram();
};

