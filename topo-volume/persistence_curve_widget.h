#pragma once

#include <map>
#include <vector>
#include <array>
#include <limits>
#include <cmath>
// include the vtk headers
#include <vtkSmartPointer.h>
#include <vtkPersistenceCurve.h>
#include <vtkXMLImageDataReader.h>
#include <vtkThreshold.h>
#include <vtkPersistenceDiagram.h>
#include <vtkTopologicalSimplification.h>
// include the local headers
#include <glm/glm.hpp>
#include "imgui-1.49/imgui.h"

/**
 * @brief render persistence curve
 * @reference https://github.com/ocornut/imgui/wiki/plot_var_example
 */
class PersistenceCurveWidget {
	vtkSmartPointer<vtkPersistenceDiagram> diagram;
	vtkSmartPointer<vtkThreshold> critical_pairs, persistent_pairs;
	vtkSmartPointer<vtkTopologicalSimplification> simplification;
    vtkSmartPointer<vtkPersistenceCurve> vtkcurve;
	ttk::TreeType tree_type;

	// Data for the ui display
	std::vector<glm::vec2> curve_points;
	glm::vec2 persistence_range, npairs_range, threshold_range;

public:
	/* Setup the persistence curve display for the passed volume data. The
	 * topological simplification selected by the user can then be gotten
	 * via `get_simplification`
	 */
    PersistenceCurveWidget(vtkSmartPointer<vtkXMLImageDataReader> data);
	// Get the topological simplification resulting from the user's selection
	vtkTopologicalSimplification* get_simplification() const;
    /**
     * @brief plot curve
     */
    void draw_ui();
	// Set the tree type we should be showing the persistence curve for
	void set_tree_type(const ttk::TreeType &type);

private:
	void draw_persistence_curve();
	void draw_persistence_diagram();
    /**
     * @brief compute curve
     */
    void update();
};

