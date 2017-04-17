/*=========================================================================
  VTK LICENSE
  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

/* This is a hacked version of the ExternalOpenGLRenderer
 * such that it actually works properly.
 */

#pragma once

#include <glm/glm.hpp>
#include "vtkRenderingExternalModule.h" // For export macro
#include "vtkOpenGLRenderer.h"

// Forward declarations
class vtkLightCollection;
class ExternalLight;

class ExternalOpenGLRenderer : public vtkOpenGLRenderer {
public:
	static ExternalOpenGLRenderer *New();
	vtkTypeMacro(ExternalOpenGLRenderer, vtkOpenGLRenderer);
	void PrintSelf(ostream& os, vtkIndent indent);

	/**
	 * Synchronize camera and light parameters
	 */
	void Render(void);

	/**
	 * Create a new Camera sutible for use with this type of Renderer.
	 * This function creates the ExternalOpenGLCamera.
	 */
	vtkCamera* MakeCamera();

	void set_camera(const glm::mat4 &proj, const glm::mat4 &view);

protected:
	ExternalOpenGLRenderer();
	~ExternalOpenGLRenderer();

	glm::dmat4 proj_mat, view_mat;

private:
	ExternalOpenGLRenderer(const ExternalOpenGLRenderer&) VTK_DELETE_FUNCTION;
	void operator=(const ExternalOpenGLRenderer&) VTK_DELETE_FUNCTION;
};

