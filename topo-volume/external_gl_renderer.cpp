/*=========================================================================
  VTK LICENSE
  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "external_gl_renderer.h"
#include <glm/ext.hpp>

#include "vtkCamera.h"
#include "vtkCommand.h"
#include "vtkExternalLight.h"
#include "vtkExternalOpenGLCamera.h"
#include "vtkLightCollection.h"
#include "vtkLightCollection.h"
#include "vtkLight.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGL.h"
#include "vtkRenderWindow.h"
#include "vtkTexture.h"

vtkStandardNewMacro(ExternalOpenGLRenderer);

ExternalOpenGLRenderer::ExternalOpenGLRenderer() {
	this->PreserveColorBuffer = 0;
	this->PreserveDepthBuffer = 0;
	this->SetAutomaticLightCreation(1);
	proj_mat = glm::dmat4(1);
	view_mat = glm::dmat4(1);
}
ExternalOpenGLRenderer::~ExternalOpenGLRenderer() {}
void ExternalOpenGLRenderer::Render(void) {
	vtkExternalOpenGLCamera* camera = vtkExternalOpenGLCamera::SafeDownCast(
			this->GetActiveCameraAndResetIfCreated());

	camera->SetProjectionTransformMatrix(glm::value_ptr(proj_mat));
	camera->SetViewTransformMatrix(glm::value_ptr(view_mat));

	vtkMatrix4x4* matrix = vtkMatrix4x4::New();
	matrix->DeepCopy(glm::value_ptr(view_mat));
	matrix->Transpose();
	matrix->Invert();

	// Synchronize camera viewUp
	double viewUp[4] = {0.0, 1.0, 0.0, 0.0}, newViewUp[4];
	matrix->MultiplyPoint(viewUp, newViewUp);
	vtkMath::Normalize(newViewUp);
	camera->SetViewUp(newViewUp);

	// Synchronize camera position
	double position[4] = {0.0, 0.0, 1.0, 1.0}, newPosition[4];
	matrix->MultiplyPoint(position, newPosition);

	if (newPosition[3] != 0.0) {
		newPosition[0] /= newPosition[3];
		newPosition[1] /= newPosition[3];
		newPosition[2] /= newPosition[3];
		newPosition[3] = 1.0;
	}
	camera->SetPosition(newPosition);

	// Synchronize focal point
	double focalPoint[4] = {0.0, 0.0, 0.0, 1.0}, newFocalPoint[4];
	matrix->MultiplyPoint(focalPoint, newFocalPoint);
	camera->SetFocalPoint(newFocalPoint);
	matrix->Delete();

	// Forward the call to the Superclass
	this->Superclass::Render();
}
vtkCamera* ExternalOpenGLRenderer::MakeCamera() {
	vtkCamera* cam = vtkExternalOpenGLCamera::New();
	this->InvokeEvent(vtkCommand::CreateCameraEvent, cam);
	return cam;
}
void ExternalOpenGLRenderer::set_camera(const glm::mat4 &proj, const glm::mat4 &view) {
	proj_mat = proj;
	view_mat = view;
}
void ExternalOpenGLRenderer::PrintSelf(ostream &os, vtkIndent indent) {
	this->Superclass::PrintSelf(os, indent);
}

