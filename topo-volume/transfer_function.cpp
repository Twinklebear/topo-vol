#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include "imgui-1.49/imgui.h"
#include <SDL.h>
#include <glm/ext.hpp>
#include "glt/util.h"
#include "volume.h"
#include "transfer_function.h"

TransferFunction::Line::Line() : line({glm::vec2(0, 0), glm::vec2(1, 1)}), color(0xffffffff) {}
void TransferFunction::Line::move_point(const float &start_x, const glm::vec2 &end){
	// Find the closest point to where the user clicked
	auto fnd = std::min_element(line.begin(), line.end(),
		[&start_x](const glm::vec2 &a, const glm::vec2 &b){
			return std::abs(start_x - a.x) < std::abs(start_x - b.x);
		});
	// If there's no nearby point we need to insert a new one
	// TODO: How much fudge to allow for here?
	if (std::abs(start_x - fnd->x) >= 0.01){
		std::vector<glm::vec2>::iterator split = line.begin();
		for (; split != line.end(); ++split){
			if (split->x < start_x && start_x < (split + 1)->x){
				break;
			}
		}
		assert(split != line.end());
		line.insert(split + 1, end);
	} else {
		*fnd = end;
		// Keep the start and end points clamped to the left/right side
		if (fnd == line.begin()){
			fnd->x = 0;
		} else if (fnd == line.end() - 1){
			fnd->x = 1;
		} else {
			// If it's a point in the middle keep it from going past its neighbors
			fnd->x = glm::clamp(fnd->x, (fnd - 1)->x, (fnd + 1)->x);
		}
	}
}
void TransferFunction::Line::remove_point(const float &x){
	if (line.size() == 2){
		return;
	}
	// See if we have a segment starting near that point
	auto fnd = std::min_element(line.begin(), line.end(),
		[&x](const glm::vec2 &a, const glm::vec2 &b){
			return std::abs(x - a.x) < std::abs(x - b.x);
		});
	// Don't allow erasure of the start and end points of the line
	if (fnd != line.end() && fnd + 1 != line.end() && fnd != line.begin()){
		line.erase(fnd);
	}
}

TransferFunction::TransferFunction() : active_line(3), fcn_changed(true), palette_tex({0, 0}) {
	rgba_lines[0].color = 0xff0000ff;
	rgba_lines[1].color = 0xff00ff00;
	rgba_lines[2].color = 0xffff0000;
	rgba_lines[3].color = 0xffffffff;

	// Colors similar to ParaView's cool-warm colormap
	std::vector<glm::vec3> default_colors{
		glm::vec3(0.231373, 0.298039, 0.752941),
		glm::vec3(0.865003, 0.865003, 0.865003),
		glm::vec3(0.705882, 0.0156863, 0.14902)};

	for (size_t i = 0; i < 3; ++i) {
		rgba_lines[i].line.clear();
	}
	const float n_colors = static_cast<float>(default_colors.size()) - 1;
	for (size_t i = 0; i < default_colors.size(); ++i) {
		for (size_t j = 0; j < 3; ++j) {
			rgba_lines[j].line.push_back(glm::vec2(i / n_colors, default_colors[i][j]));
		}
	}
}
TransferFunction::~TransferFunction(){
	if (palette_tex[0]){
		glDeleteTextures(2, palette_tex.data());
	}
}
void TransferFunction::draw_ui(){
	if (ImGui::Begin("Transfer Function")){
		ImGui::Text("Left click and drag to add/move points\nRight click to remove\n");
		ImGui::RadioButton("Red", &active_line, 0); ImGui::SameLine();
		ImGui::RadioButton("Green", &active_line, 1); ImGui::SameLine();
		ImGui::RadioButton("Blue", &active_line, 2); ImGui::SameLine();
		ImGui::RadioButton("Alpha", &active_line, 3);

		glm::vec2 canvas_pos(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
		glm::vec2 canvas_size(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
		// Force some min size of the editor
		if (canvas_size.x < 50.f){
			canvas_size.x = 50.f;
		}
		if (canvas_size.y < 50.f){
			canvas_size.y = 50.f;
		}

		if (palette_tex[0]){
			ImGui::Image(reinterpret_cast<void*>(palette_tex[1]), ImVec2(canvas_size.x, 16));
			canvas_pos.y += 20;
			canvas_size.y -= 20;
		}

		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		draw_list->AddRect(canvas_pos, canvas_pos + canvas_size, ImColor(255, 255, 255));

		const glm::vec2 view_scale(canvas_size.x, -canvas_size.y);
		const glm::vec2 view_offset(canvas_pos.x, canvas_pos.y + canvas_size.y);

		ImGui::InvisibleButton("canvas", canvas_size);
		if (ImGui::IsItemHovered()){
			glm::vec2 mouse_pos = glm::vec2(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
			mouse_pos = (mouse_pos - view_offset) / view_scale;
			// Need to somehow find which line of RGBA the mouse is closest too
			if (ImGui::GetIO().MouseDown[0]){
				rgba_lines[active_line].move_point(mouse_pos.x, mouse_pos);
				fcn_changed = true;
			} else if (ImGui::IsMouseClicked(1)){
				rgba_lines[active_line].remove_point(mouse_pos.x);
				fcn_changed = true;
			}
		}
		draw_list->PushClipRect(canvas_pos, canvas_pos + canvas_size);
		if (!histogram.empty()){
			const size_t max_val = *std::max_element(histogram.begin(), histogram.end());
			const float bar_width = 1.0f / static_cast<float>(histogram.size());
			for (size_t i = 0; i < histogram.size(); ++i){
				glm::vec2 bottom{bar_width * i, 0.f};
				glm::vec2 top{bottom.x + bar_width, histogram[i] / static_cast<float>(max_val)};
				draw_list->AddRectFilled(view_offset + view_scale * bottom, view_offset + view_scale * top, 0xffaaaaaa);
			}
		}

		// TODO: Should also draw little boxes showing the clickable region for each
		// line segment
		for (int i = 0; i < static_cast<int>(rgba_lines.size()); ++i){
			if (i == active_line){
				continue;
			}
			for (size_t j = 0; j < rgba_lines[i].line.size() - 1; ++j){
				const glm::vec2 &a = rgba_lines[i].line[j];
				const glm::vec2 &b = rgba_lines[i].line[j + 1];
				draw_list->AddLine(view_offset + view_scale * a, view_offset + view_scale * b,
						rgba_lines[i].color, 2.0f);
			}
		}
		// Draw the active line on top
		for (size_t j = 0; j < rgba_lines[active_line].line.size() - 1; ++j){
			const glm::vec2 &a = rgba_lines[active_line].line[j];
			const glm::vec2 &b = rgba_lines[active_line].line[j + 1];
			draw_list->AddLine(view_offset + view_scale * a, view_offset + view_scale * b,
					rgba_lines[active_line].color, 2.0f);
		}
		draw_list->PopClipRect();
	}
	ImGui::End();
}
void TransferFunction::render(){
	// TODO: How many samples for a palette? 128 or 256 is probably plent
	const int samples = 256;
	// Upload to GL if the transfer function has changed
	if (!palette_tex[0]){
		glGenTextures(2, palette_tex.data());
		// How to pick what texture unit we're on?
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_1D, palette_tex[0]);
		glTexStorage1D(GL_TEXTURE_1D, 1, GL_RGBA8, samples);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, palette_tex[1]);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, samples, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if (fcn_changed){
		// Sample the palette then upload the data
		std::vector<uint8_t> palette(samples * 4, 0);
		// Step along the alpha line and sample it
		std::array<std::vector<glm::vec2>::const_iterator, 4> lit = {
			rgba_lines[0].line.begin(), rgba_lines[1].line.begin(),
			rgba_lines[2].line.begin(), rgba_lines[3].line.begin()
		};
		float step = 1.0 / samples;
		for (size_t i = 0; i < samples; ++i){
			float x = step * i;
			for (size_t j = 0; j < lit.size(); ++j){
				if (x > (lit[j] + 1)->x){
					++lit[j];
				}
				assert(lit[j] != rgba_lines[j].line.end());
				float t = (x - lit[j]->x) / ((lit[j] + 1)->x - lit[j]->x);
				palette[i * 4 + j] = static_cast<uint8_t>(glm::lerp(lit[j]->y, (lit[j] + 1)->y, t) * 255.0);
			}
		}
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_1D, palette_tex[0]);
		glTexSubImage1D(GL_TEXTURE_1D, 0, 0, samples, GL_RGBA, GL_UNSIGNED_BYTE,
				static_cast<const void*>(palette.data()));

		// Go through and set alpha to all 1 and gamma correct the sample color values
		lit[0] = rgba_lines[0].line.begin();
		lit[1] = rgba_lines[1].line.begin();
		lit[2] = rgba_lines[2].line.begin();
		for (size_t i = 0; i < samples; ++i){
			float x = step * i;
			for (size_t j = 0; j < 3; ++j){
				if (x > (lit[j] + 1)->x){
					++lit[j];
				}
				assert(lit[j] != rgba_lines[j].line.end());
				float t = (x - lit[j]->x) / ((lit[j] + 1)->x - lit[j]->x);
				palette[i * 4 + j] = static_cast<uint8_t>(glm::lerp(lit[j]->y, (lit[j] + 1)->y, t) * 255.0);
			}
		}
		for (size_t i = 0; i < samples; ++i){
			palette[i * 4 + 3] = 255;
		}
		glBindTexture(GL_TEXTURE_2D, palette_tex[1]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, samples, 1, GL_RGBA, GL_UNSIGNED_BYTE,
				static_cast<const void*>(palette.data()));
		fcn_changed = false;
	}
	// TODO: Bindless textures?
	// Instead of this the palette should send its texture name to the volume
	// so it can take care of finding it properly when the volume is rendered
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, palette_tex[0]);
}

