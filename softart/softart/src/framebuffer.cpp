/*
Copyright (C) 2007-2010 Minmin Gong, Ye Wu

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "eflib/include/eflib.h"
#include "eflib/include/util.h"

#include <algorithm>

#include "../include/framebuffer.h"
#include "../include/surface.h"
#include "../include/renderer_impl.h"
#include "../include/shader.h"

BEGIN_NS_SOFTART()


using namespace efl;
using namespace std;

int32_t mask_stencil_0(int32_t /*stencil*/, int32_t /*mask*/){
	return 0;
}

int32_t mask_stencil_1(int32_t stencil, int32_t mask){
	return stencil & mask;
}

int32_t read_stencil_0(const backbuffer_pixel_out& /*target_pixel*/, size_t /*sample*/, int32_t /*mask*/){
	return 0;
}

int32_t read_stencil_1(const backbuffer_pixel_out& target_pixel, size_t sample, int32_t mask){
	return target_pixel.stencil(sample) & mask;
}


template <typename T>
bool cmp_func_never(T /*lhs*/, T /*rhs*/){
	return false;
}

template <typename T>
bool cmp_func_less(T lhs, T rhs){
	return lhs < rhs;
}

template <typename T>
bool cmp_func_equal(T lhs, T rhs){
	return lhs == rhs;
}

template <typename T>
bool cmp_func_less_equal(T lhs, T rhs){
	return lhs <= rhs;
}

template <typename T>
bool cmp_func_greater(T lhs, T rhs){
	return lhs > rhs;
}

template <typename T>
bool cmp_func_not_equal(T lhs, T rhs){
	return lhs != rhs;
}

template <typename T>
bool cmp_func_greater_equal(T lhs, T rhs){
	return lhs >= rhs;
}

template <typename T>
bool cmp_func_always(T /*lhs*/, T /*rhs*/){
	return true;
}

int32_t sop_keep(int32_t /*ref*/, int32_t cur_stencil){
	return cur_stencil;
}

int32_t sop_zero(int32_t /*ref*/, int32_t /*cur_stencil*/){
	return 0;
}

int32_t sop_replace(int32_t ref, int32_t /*cur_stencil*/){
	return ref;
}

int32_t sop_incr_sat(int32_t /*ref*/, int32_t cur_stencil){
	return std::min<int32_t>(0xFF, cur_stencil + 1);
}

int32_t sop_decr_sat(int32_t /*ref*/, int32_t cur_stencil){
	return std::max<int32_t>(0, cur_stencil - 1);
}

int32_t sop_invert(int32_t /*ref*/, int32_t cur_stencil){
	return ~cur_stencil;
}

int32_t sop_incr_wrap(int32_t /*ref*/, int32_t cur_stencil){
	return (cur_stencil + 1) & 0xFF;
}

int32_t sop_decr_wrap(int32_t /*ref*/, int32_t cur_stencil){
	return (cur_stencil - 1 + 256) & 0xFF;
}

void write_depth_depth_mask_0(size_t /*sample*/, float /*depth*/, backbuffer_pixel_out& /*target_pixel*/){
}

void write_depth_depth_mask_1(size_t sample, float depth, backbuffer_pixel_out& target_pixel){
	target_pixel.depth(sample, depth);
}

void write_stencil_0(size_t /*sample*/, int32_t /*stencil*/, int32_t /*mask*/, backbuffer_pixel_out& /*target_pixel*/){
}

void write_stencil_1(size_t sample, int32_t stencil, int32_t mask, backbuffer_pixel_out& target_pixel){
	target_pixel.stencil(sample, stencil & mask);
}

depth_stencil_state::depth_stencil_state(const depth_stencil_desc& desc)
	: desc_(desc)
{
	if (desc.depth_enable){
		switch (desc.depth_func){
		case compare_function_never:
			depth_test_func_ = cmp_func_never<float>;
			break;
		case compare_function_less:
			depth_test_func_ = cmp_func_less<float>;
			break;
		case compare_function_equal:
			depth_test_func_ = cmp_func_equal<float>;
			break;
		case compare_function_less_equal:
			depth_test_func_ = cmp_func_less_equal<float>;
			break;
		case compare_function_greater:
			depth_test_func_ = cmp_func_greater<float>;
			break;
		case compare_function_not_equal:
			depth_test_func_ = cmp_func_not_equal<float>;
			break;
		case compare_function_greater_equal:
			depth_test_func_ = cmp_func_greater_equal<float>;
			break;
		case compare_function_always:
			depth_test_func_ = cmp_func_always<float>;
			break;
		default:
			custom_assert(false, "");
			break;
		}
	}
	else{
		depth_test_func_ = cmp_func_always<float>;
	}

	if (desc.stencil_enable){
		mask_stencil_func_ = mask_stencil_1;
		read_stencil_func_ = read_stencil_1;

		compare_function stencil_funcs[2] = {
			desc.front_face.stencil_func,
			desc.back_face.stencil_func
		};
		for (int i = 0; i < 2; ++ i){
			switch (stencil_funcs[i]){
			case compare_function_never:
				stencil_test_func_[i] = cmp_func_never<int32_t>;
				break;
			case compare_function_less:
				stencil_test_func_[i] = cmp_func_less<int32_t>;
				break;
			case compare_function_equal:
				stencil_test_func_[i] = cmp_func_equal<int32_t>;
				break;
			case compare_function_less_equal:
				stencil_test_func_[i] = cmp_func_less_equal<int32_t>;
				break;
			case compare_function_greater:
				stencil_test_func_[i] = cmp_func_greater<int32_t>;
				break;
			case compare_function_not_equal:
				stencil_test_func_[i] = cmp_func_not_equal<int32_t>;
				break;
			case compare_function_greater_equal:
				stencil_test_func_[i] = cmp_func_greater_equal<int32_t>;
				break;
			case compare_function_always:
				stencil_test_func_[i] = cmp_func_always<int32_t>;
				break;
			default:
				custom_assert(false, "");
				break;
			}
		}

		stencil_op sops[6] = {
			desc.front_face.stencil_fail_op,
			desc.front_face.stencil_pass_op,
			desc.front_face.stencil_depth_fail_op,
			desc.back_face.stencil_fail_op,
			desc.back_face.stencil_pass_op,
			desc.back_face.stencil_depth_fail_op
		};
		for (int i = 0; i < 6; ++ i){
			switch (sops[i]){
			case stencil_op_keep:
				stencil_op_func_[i] = sop_keep;
				break;
			case stencil_op_zero:
				stencil_op_func_[i] = sop_zero;
				break;
			case stencil_op_replace:
				stencil_op_func_[i] = sop_replace;
				break;
			case stencil_op_incr_sat:
				stencil_op_func_[i] = sop_incr_sat;
				break;
			case stencil_op_decr_sat:
				stencil_op_func_[i] = sop_decr_sat;
				break;
			case stencil_op_invert:
				stencil_op_func_[i] = sop_invert;
				break;
			case stencil_op_incr_wrap:
				stencil_op_func_[i] = sop_incr_wrap;
				break;
			case stencil_op_decr_wrap:
				stencil_op_func_[i] = sop_decr_wrap;
				break;
			default:
				custom_assert(false, "");
				break;
			}
		}
	}
	else{
		mask_stencil_func_ = mask_stencil_0;
		read_stencil_func_ = read_stencil_0;

		for (int i = 0; i < 2; ++ i){
			stencil_test_func_[i] = cmp_func_always<int32_t>;
		}

		for (int i = 0; i < 6; ++ i){
			stencil_op_func_[i] = sop_keep;
		}
	}

	if (desc.depth_write_mask){
		write_depth_func_ = write_depth_depth_mask_1;
	}
	else{
		write_depth_func_ = write_depth_depth_mask_0;
	}
	if (desc.stencil_enable){
		write_stencil_func_ = write_stencil_1;
	}
	else{
		write_stencil_func_ = write_stencil_0;
	}
}

const depth_stencil_desc& depth_stencil_state::get_desc() const{
	return desc_;
}

int32_t depth_stencil_state::read_stencil(int32_t stencil) const{
	return mask_stencil_func_(stencil, desc_.stencil_read_mask);
}

int32_t depth_stencil_state::read_stencil(const backbuffer_pixel_out& target_pixel, size_t sample) const{
	return read_stencil_func_(target_pixel, sample, desc_.stencil_read_mask);
}

bool depth_stencil_state::depth_test(float ps_depth, float cur_depth) const{
	return depth_test_func_(ps_depth, cur_depth);
}

bool depth_stencil_state::stencil_test(bool front_face, int32_t ref, int32_t cur_stencil) const{
	return stencil_test_func_[!front_face](ref, cur_stencil);
}

int32_t depth_stencil_state::stencil_operation(bool front_face, bool depth_pass, bool stencil_pass, int32_t ref, int32_t cur_stencil) const{
	return stencil_op_func_[(!front_face) * 3 + (!depth_pass) + static_cast<int>(stencil_pass)](ref, cur_stencil);
}

void depth_stencil_state::write_depth(size_t sample, float depth, backbuffer_pixel_out& target_pixel) const{
	write_depth_func_(sample, depth, target_pixel);
}

void depth_stencil_state::write_stencil(size_t sample, int32_t stencil, backbuffer_pixel_out& target_pixel) const{
	write_stencil_func_(sample, stencil, desc_.stencil_write_mask, target_pixel);
}

/****************************************************
 *   Framebuffer
 ***************************************************/
bool framebuffer::check_buf(surface* psurf){
	return psurf && (psurf->get_width() < width_ || psurf->get_height() < height_);
}

void framebuffer::initialize(renderer_impl* pparent)
{
	pparent_ = pparent;
}

framebuffer::framebuffer(size_t width, size_t height, size_t num_samples, pixel_format fmt)
:width_(width), height_(height), num_samples_(num_samples), fmt_(fmt),
back_cbufs_(pso_color_regcnt), cbufs_(pso_color_regcnt), buf_valids(pso_color_regcnt),
dbuf_(new surface(width, height, num_samples, pixel_format_color_r32f)),
sbuf_(new surface(width, height, num_samples, pixel_format_color_r32i))
{
	for(size_t i = 0; i < back_cbufs_.size(); ++i){
		back_cbufs_[i].reset();
	}

	back_cbufs_[0].reset(new surface(width, height, num_samples, fmt));

	for(size_t i = 0; i < cbufs_.size(); ++i){
		cbufs_[i] = back_cbufs_[i].get();
	}

	buf_valids[0] = true;
	for(size_t i = 1; i < buf_valids.size(); ++i){
		buf_valids[i] = false;
	}
}

framebuffer::~framebuffer(void)
{
}

//���ã���һ��RT������Ϊ֡������棬����RT�ÿ�
void framebuffer::reset(size_t width, size_t height, size_t num_samples, pixel_format fmt)
{
	new(this) framebuffer(width, height, num_samples, fmt);
}

void framebuffer::set_render_target_disabled(render_target tar, size_t tar_id){
	custom_assert(tar == render_target_color, "ֻ�ܽ�����ɫ����");
	custom_assert(tar_id < cbufs_.size(), "��ɫ����ID�����ô���");
	UNREF_PARAM(tar);

	//�򵥵�����Ϊ��Ч
	buf_valids[tar_id] = false;
}

void framebuffer::set_render_target_enabled(render_target tar, size_t tar_id){
	custom_assert(tar == render_target_color, "ֻ��������ɫ����");
	custom_assert(tar_id < cbufs_.size(), "��ɫ����ID�����ô���");
	UNREF_PARAM(tar);

	//�ط���󻺳�
	if(back_cbufs_[tar_id] && check_buf(back_cbufs_[tar_id].get())){
		back_cbufs_[tar_id].reset(new surface(width_, height_, num_samples_, fmt_));
	}

	//�����ȾĿ��Ϊ�����Զ��ҽӺ󻺳�
	if(cbufs_[tar_id] == NULL){
		cbufs_[tar_id] = back_cbufs_[tar_id].get();
	}

	//���󻺳���Ч��
	if(check_buf(cbufs_[tar_id])){
		custom_assert(false, "Ŀ�껺����Ч��");
		cbufs_[tar_id] = back_cbufs_[tar_id].get();
	}

	//������Ч��
	buf_valids[tar_id] = true;
}

//��ȾĿ�����á���ʱ��֧��depth buffer��stencil buffer�İ󶨺Ͷ�ȡ��
void framebuffer::set_render_target(render_target tar, size_t tar_id, surface* psurf)
{
	custom_assert(tar == render_target_color, "ֻ�ܰ���ɫ����");
	custom_assert(tar_id < cbufs_.size(), "��ɫ����ID�İ󶨴���");
	UNREF_PARAM(tar);

	//�������ı���Ϊ����ָ���ȾĿ��Ϊ�󱸻���
	if(!psurf){
		cbufs_[tar_id] = back_cbufs_[tar_id].get();
		return;
	}

	custom_assert(psurf->get_width() >= width_ && psurf->get_height() >= height_, "��ȾĿ��Ĵ�С����");
	cbufs_[tar_id] = psurf;
}

surface* framebuffer::get_render_target(render_target tar, size_t tar_id) const
{
	custom_assert(tar == render_target_color, "ֻ�ܻ����ɫ����");
	custom_assert(tar_id < cbufs_.size(), "��ɫ����ID���ô���");
	UNREF_PARAM(tar);

	return cbufs_[tar_id];
}
	
rect<size_t> framebuffer::get_rect(){
		return rect<size_t>(0, 0, width_, height_);
	}

size_t framebuffer::get_width() const{
	return width_;
}

size_t framebuffer::get_height() const{
	return height_;
}

size_t framebuffer::get_num_samples() const{
	return num_samples_;
}

pixel_format framebuffer::get_buffer_format() const{
	return fmt_;
}

//��Ⱦ
void framebuffer::render_sample(const h_blend_shader& hbs, size_t x, size_t y, size_t i_sample, const ps_output& ps, float depth)
{
	custom_assert(hbs, "δ����Target Shader!");
	if(! hbs) return;

	//composing output...
	backbuffer_pixel_out target_pixel(cbufs_, dbuf_.get(), sbuf_.get());
	target_pixel.set_pos(x, y);

	const h_depth_stencil_state& dss = pparent_->get_depth_stencil_state();

	const int32_t stencil_ref = dss->read_stencil(pparent_->get_stencil_ref());
	
	const float cur_depth = target_pixel.depth(i_sample);
	const int32_t cur_stencil = dss->read_stencil(target_pixel, i_sample);

	bool depth_passed = dss->depth_test(depth, cur_depth);
	bool stencil_passed = dss->stencil_test(ps.front_face, stencil_ref, cur_stencil);

	if (depth_passed && stencil_passed){
		int32_t new_stencil = dss->stencil_operation(ps.front_face, depth_passed, stencil_passed, stencil_ref, cur_stencil);

		//execute target shader
		hbs->execute(i_sample, target_pixel, ps);

		dss->write_depth(i_sample, depth, target_pixel);
		dss->write_stencil(i_sample, new_stencil, target_pixel);
	}
}

void framebuffer::clear_color(size_t tar_id, const color_rgba32f& c){

	custom_assert(tar_id < cbufs_.size(), "��ȾĿ���ʶ���ô���");
	custom_assert(cbufs_[tar_id] && buf_valids[tar_id], "��ͼ��һ����Ч����ȾĿ��������ɫ��");

	cbufs_[tar_id]->fill_texels(0, 0, width_, height_, c);
}

void framebuffer::clear_depth(float d){
	dbuf_->fill_texels(0, 0, width_, height_, color_rgba32f(d, 0, 0, 0));
}

void framebuffer::clear_stencil(int32_t s){
	sbuf_->fill_texels(0, 0, width_, height_, color_rgba32f(float(s), 0, 0, 0));
}

void framebuffer::clear_color(size_t tar_id, const rect<size_t>& rc, const color_rgba32f& c){

	custom_assert(tar_id < cbufs_.size(), "��ȾĿ���ʶ���ô���");
	custom_assert(cbufs_[tar_id] && buf_valids[tar_id], "��ͼ��һ����Ч����ȾĿ��������ɫ��");
	custom_assert(rc.w + rc.x <= width_ && rc.h +rc.y <= height_, "�������򳬹���֡���巶Χ��");

	cbufs_[tar_id]->fill_texels(rc.x, rc.y, rc.w, rc.h, c);
}

void framebuffer::clear_depth(const rect<size_t>& rc, float d){
	custom_assert(rc.w + rc.x <= width_ && rc.h +rc.y <= height_, "�������򳬹���֡���巶Χ��");

	dbuf_->fill_texels(rc.x, rc.y, rc.w, rc.h, color_rgba32f(d, 0, 0, 0));
}

void framebuffer::clear_stencil(const rect<size_t>& rc, int32_t s){
	custom_assert(rc.w + rc.x <= width_ && rc.h +rc.y <= height_, "�������򳬹���֡���巶Χ��");

	sbuf_->fill_texels(rc.x, rc.y, rc.w, rc.h, color_rgba32f(float(s), 0, 0, 0));
}

END_NS_SOFTART()
