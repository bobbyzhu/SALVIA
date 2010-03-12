#ifndef SOFTART_TEXTURE_H
#define SOFTART_TEXTURE_H

#include "surface.h"
#include "colors.h"

#include "enums.h"
#include "decl.h"

#include <boost/smart_ptr.hpp>
#include <vector>
#include "softart_fwd.h"
BEGIN_NS_SOFTART()


class texture
{
protected:
	pixel_format fmt_;
	size_t min_lod_;
	size_t max_lod_;

	static size_t calc_lod_limit(size_t x, size_t y = 1, size_t z = 1)
	{
		custom_assert(x > 0 && y > 0 && z > 0, "");

		int rv = 0;
		while (x > 0 || y > 0 || z > 0){
			x >>= 1;
			y >>= 1;
			z >>= 1;
			++rv;
		}

		return rv;
	}
public:
	texture():max_lod_(0), min_lod_(0){}

	size_t get_min_lod() const{return min_lod_;}
	size_t get_max_lod() const{return max_lod_;}
	pixel_format get_pixel_format() const{return fmt_;}

	virtual void lock(void** pData, size_t miplevel, const efl::rect<size_t>& lrc, lock_mode lm, size_t z_slice = 0) = 0;
	virtual void unlock() = 0;

	virtual surface& get_surface(size_t miplevel, size_t z_slice = 0) = 0;
	virtual const surface& get_surface(size_t miplevel, size_t z_slice = 0) const = 0;

	virtual size_t get_width(size_t miplevel) const= 0;
	virtual size_t get_height(size_t miplevel) const= 0;
	virtual size_t get_depth(size_t miplevel) const= 0;
	
	virtual void set_max_lod(size_t miplevel) = 0;
	virtual void set_min_lod(size_t miplevel) = 0;

	virtual void gen_mipmap(filter_type filter) = 0;
};

class texture_2d : public texture
{
	std::vector<surface> surfs_;
	size_t locked_surface_;
	bool is_locked_;

	size_t width_;
	size_t height_;
public:
	texture_2d(size_t width, size_t height, pixel_format format);
	void reset(size_t width, size_t height, pixel_format format);

	virtual void gen_mipmap(filter_type filter);

	virtual void lock(void** pData, size_t miplevel, const efl::rect<size_t>& lrc, lock_mode lm, size_t z_slice = 0);
	virtual void unlock();

	virtual surface& get_surface(size_t miplevel, size_t z_slice = 0);
	virtual const surface& get_surface(size_t miplevel, size_t z_slice = 0) const;

	virtual size_t get_width(size_t mipmap = 0) const;
	virtual size_t get_height(size_t mipmap = 0) const;
	virtual size_t get_depth(size_t mipmap = 0) const;
	
	virtual void set_max_lod(size_t miplevel);
	virtual void set_min_lod(size_t miplevel);
};

class texture_cube : public texture
{
	std::vector<texture_2d> subtexs_;
	size_t locked_texture_;
	bool is_locked_;

	size_t width_;
	size_t height_;
public:
	texture_cube(size_t width, size_t height, pixel_format format);
	void reset(size_t width, size_t height, pixel_format format);

	virtual void gen_mipmap(filter_type filter);

	virtual void lock(void** pData, size_t miplevel, const efl::rect<size_t>& lrc, lock_mode lm, size_t z_slice = 0);
	virtual void unlock();

	virtual surface& get_surface(size_t miplevel, size_t z_slice = 0);
	virtual const surface& get_surface(size_t miplevel, size_t z_slice = 0) const;

	virtual surface& get_surface(size_t miplevel, cubemap_faces face);
	virtual const surface& get_surface(size_t miplevel, cubemap_faces face) const;

	virtual texture& get_face(cubemap_faces face);
	virtual const texture& get_face(cubemap_faces face) const;

	virtual size_t get_width(size_t mipmap = 0) const;
	virtual size_t get_height(size_t mipmap = 0) const;
	virtual size_t get_depth(size_t mipmap = 0) const;
	
	virtual void set_max_lod(size_t miplevel);
	virtual void set_min_lod(size_t miplevel);
};

END_NS_SOFTART()

#endif