#ifndef SALVIAR_RESOURCE_MANAGER_H
#define SALVIAR_RESOURCE_MANAGER_H

#include "buffer.h"
#include "texture.h"
#include <salviar/include/salviar_forward.h>
BEGIN_NS_SALVIAR()


class buffer_manager
{
public:
	h_buffer create_buffer(size_t size){
		return boost::shared_ptr<buffer>(new buffer(size));
	}

	void release_buffer(h_buffer& hbuf){
		if(hbuf){
			hbuf.reset();
			return;
		}
		EFLIB_ASSERT(false, "");
	}
};

class texture_manager
{
public:
	h_texture create_texture_2d(size_t width, size_t height, size_t num_samples, pixel_format fmt){
		return h_texture(new texture_2d(width, height, num_samples, fmt));
	}
	h_texture create_texture_cube(size_t width, size_t height, size_t num_samples, pixel_format fmt){
		return h_texture(new texture_cube(width, height, num_samples, fmt));
	}

	void release_texture(h_texture& htex){
		if(htex){
			htex.reset();
			return;
		}
		EFLIB_ASSERT(false, "");
	}
};

END_NS_SALVIAR()

#endif