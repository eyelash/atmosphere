/*

Copyright (c) 2016-2017, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "nitro.hpp"

// Padding
nitro::Padding::Padding(float padding): padding(padding) {

}
void nitro::Padding::layout() {
	if (Node* child = get_child()) {
		child->set_location(padding, padding);
		child->set_size(get_width() - 2.f * padding, get_height() - 2.f * padding);
	}
}
float nitro::Padding::get_padding() const {
	return padding;
}
void nitro::Padding::set_padding(float padding) {
	if (padding == this->padding) {
		return;
	}
	this->padding = padding;
	Padding::layout();
}

// Alignment
nitro::Alignment::Alignment(HorizontalAlignment horizontal_alignment, VerticalAlignment vertical_alignment): horizontal_alignment(horizontal_alignment), vertical_alignment(vertical_alignment) {

}
void nitro::Alignment::layout() {
	Node* child = get_child();
	if (child == nullptr) {
		return;
	}
	float x, y;
	switch (horizontal_alignment) {
	case HorizontalAlignment::LEFT:
		x = 0.f;
		break;
	case HorizontalAlignment::CENTER:
		x = roundf((get_width() - child->get_width()) / 2.f);
		break;
	case HorizontalAlignment::RIGHT:
		x = roundf(get_width() - child->get_width());
		break;
	}
	switch (vertical_alignment) {
	case VerticalAlignment::BOTTOM:
		y = 0.f;
		break;
	case VerticalAlignment::CENTER:
		y = roundf((get_height() - child->get_height()) / 2.f);
		break;
	case VerticalAlignment::TOP:
		y = roundf(get_height() - child->get_height());
		break;
	}
	child->set_location(x, y);
}
nitro::HorizontalAlignment nitro::Alignment::get_horizontal_alignment() const {
	return horizontal_alignment;
}
nitro::VerticalAlignment nitro::Alignment::get_vertical_alignment() const {
	return vertical_alignment;
}
void nitro::Alignment::set_alignment(HorizontalAlignment horizontal_alignment, VerticalAlignment vertical_alignment) {
	if (horizontal_alignment == this->horizontal_alignment && vertical_alignment == this->vertical_alignment) {
		return;
	}
	this->horizontal_alignment = horizontal_alignment;
	this->vertical_alignment = vertical_alignment;
	layout();
}

static float circle(float x) {
	return sqrtf(1.f - x * x);
}
static float int_circle(float x) {
	return 0.5f * (sqrtf(1.f-x*x) * x + asinf(x));
}
// computes (the area of) the intersection of the unit circle with the given rectangle (x, y, w, h)
// x, y, w and h are all assumed to be non-negative
static float rounded_corner_area(float x, float y, const float w, const float h) {
	if (x*x+y*y >= 1.f) return 0.f;
	// assert(x < 1.f && y < 1.f);

	float x1 = std::min(x + w, 1.f);
	float y1 = std::min(y + h, 1.f);
	float result = 0.f;

	const float cy1 = circle(y1);
	if (cy1 >= x1) return w * h; // completely inside
	if (cy1 > x) {
		result += (cy1 - x) * h;
		x = cy1;
	}

	const float cy = circle(y);
	if (cy < x1) x1 = cy;

	result += int_circle(x1) - int_circle(x) - (x1 - x) * y;
	return result;
}
static float rounded_corner(float radius, float x, float y, float w = 1.f, float h = 1.f) {
	w = w / radius;
	h = h / radius;
	return rounded_corner_area(x/radius, y/radius, w, h) / (w * h);
}
static nitro::Texture create_rounded_corner_texture(int radius) {
	std::vector<unsigned char> data (radius * radius);
	for (int y = 0; y < radius; ++y) {
		for (int x = 0; x < radius; ++x) {
			data[y*radius+x] = rounded_corner((float)radius, (float)x, (float)y) * 255.f + 0.5f;
		}
	}
	return nitro::Texture::create_from_data(radius, radius, 1, data.data());
}

static nitro::Texture create_rounded_border_texture(int radius, int width) {
	std::vector<unsigned char> data (radius * radius);
	for (int y = 0; y < radius; ++y) {
		for (int x = 0; x < radius; ++x) {
			const float value = rounded_corner((float)radius, (float)x, (float)y) - rounded_corner((float)(radius-width), (float)x, (float)y);
			data[y*radius+x] = value * 255.f + 0.5f;
		}
	}
	return nitro::Texture::create_from_data(radius, radius, 1, data.data());
}

static std::vector<float> create_gaussian_kernel(int radius) {
	std::vector<float> kernel (radius * 2 + 1);
	const float sigma = radius / 3.f;
	const float factor = 1.f / sqrtf(2.f * M_PI * sigma*sigma);
	for (unsigned int i = 0; i < kernel.size(); ++i) {
		const float x = (int)i - radius;
		kernel[i] = factor * expf(-x*x/(2.f*sigma*sigma));
	}
	return kernel;
}
static void blur(const std::vector<float>& kernel, std::vector<unsigned char>& buffer, float w, float h) {
	const int center = kernel.size() / 2;
	std::vector<unsigned char> tmp (buffer.size());
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			float sum = 0.f;
			for (unsigned int k = 0; k < kernel.size(); ++k) {
				int kx = x + k - center;
				if (kx < 0) kx = 0;
				else if (kx >= w) kx = w - 1;
				sum += buffer[y * w + kx] * kernel[k];
			}
			tmp[y * w + x] = sum + .5f;
		}
	}
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			float sum = 0.f;
			for (unsigned int k = 0; k < kernel.size(); ++k) {
				int ky = y + k - center;
				if (ky < 0) ky = 0;
				else if (ky >= h) ky = h - 1;
				sum += tmp[ky * w + x] * kernel[k];
			}
			buffer[y * w + x] = sum + .5f;
		}
	}
}
static nitro::Texture create_blurred_corner_texture(int radius, int blur_radius) {
	int size = radius + blur_radius * 2;
	std::vector<unsigned char> buffer (size * size);
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			const int i = y * size + x;
			const bool inside = x - blur_radius < radius && y - blur_radius < radius;
			buffer[i] = inside ? 255 : 0;
		}
	}
	for (int y = 0; y < radius; ++y) {
		for (int x = 0; x < radius; ++x) {
			const int i = (y + blur_radius) * size + (x + blur_radius);
			buffer[i] = rounded_corner(radius, x, y) * 255.f + .5f;
		}
	}
	blur(create_gaussian_kernel(blur_radius), buffer, size, size);
	return nitro::Texture::create_from_data(size, size, 1, buffer.data());
}
