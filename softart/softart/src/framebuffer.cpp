#include "../include/framebuffer.h"

#include "../include/surface.h"
#include "../include/renderer_impl.h"
#include "../include/shader.h"

#include <xmemory>

using namespace efl;
using namespace std;

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

framebuffer::framebuffer(size_t width, size_t height, pixel_format fmt)
:dbuf_(width*height), sbuf_(width*height),
width_(width), height_(height), fmt_(fmt),
back_cbufs_(pso_color_regcnt), cbufs_(pso_color_regcnt), buf_valids(pso_color_regcnt)
{
	for(size_t i = 0; i < back_cbufs_.size(); ++i){
		back_cbufs_[i].reset();
	}

	back_cbufs_[0].reset(new surface(width, height, fmt));

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
void framebuffer::reset(size_t width, size_t height, pixel_format fmt)
{
	new(this) framebuffer(width, height, fmt);
}

void framebuffer::set_render_target_disabled(render_target tar, size_t tar_id){
	custom_assert(tar == render_target_color, "ֻ�ܽ�����ɫ����");
	custom_assert(tar_id < cbufs_.size(), "��ɫ����ID�����ô���");

	//�򵥵�����Ϊ��Ч
	buf_valids[tar_id] = false;
}

void framebuffer::set_render_target_enabled(render_target tar, size_t tar_id){
	custom_assert(tar == render_target_color, "ֻ��������ɫ����");
	custom_assert(tar_id < cbufs_.size(), "��ɫ����ID�����ô���");

	//�ط���󻺳�
	if(back_cbufs_[tar_id] && check_buf(back_cbufs_[tar_id].get())){
		back_cbufs_[tar_id].reset(new surface(width_, height_, fmt_));
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

	//�������ı���Ϊ����ָ���ȾĿ��Ϊ�󱸻���
	if(!psurf){
		cbufs_[tar_id] = back_cbufs_[tar_id].get();
		return;
	}

	custom_assert(psurf->get_width() >= width_ && psurf->get_height() >= height_, "��ȾĿ��Ĵ�С����");
	cbufs_[tar_id] = psurf;
}

surface* framebuffer::get_render_target(render_target tar, size_t tar_id)
{
	custom_assert(tar == render_target_color, "ֻ�ܻ����ɫ����");
	custom_assert(tar_id < cbufs_.size(), "��ɫ����ID���ô���");

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

pixel_format framebuffer::get_buffer_format() const{
	return fmt_;
}

//��Ⱦ
void framebuffer::render_pixel(size_t x, size_t y, const ps_output& ps)
{
	h_blend_shader hbs = pparent_->get_blend_shader();
	custom_assert(hbs, "δ����Target Shader!");
	if(! hbs) return;

	//composing input...
	backbuffer_pixel_in in(ps, &sbuf_[y*width_+x]);

	//composing output...
	backbuffer_pixel_out inout(cbufs_, x, y, &dbuf_[y*width_+x], &sbuf_[y*width_+x]);

	//execute target shader
	hbs->execute(inout, in);
}

void framebuffer::clear_color(size_t tar_id, const color_rgba32f& c){

	custom_assert(tar_id < cbufs_.size(), "��ȾĿ���ʶ���ô���");
	custom_assert(cbufs_[tar_id] && buf_valids[tar_id], "��ͼ��һ����Ч����ȾĿ��������ɫ��");

	cbufs_[tar_id]->fill_clr_texels(0, 0, width_, height_, c);
}

void framebuffer::clear_depth(float d){
	for(size_t i_y = 0; i_y < height_; ++i_y){
		for(size_t i_x = 0; i_x < width_; ++i_x){
			dbuf_[i_y*width_ + i_x] = d;
		}
	}
}

void framebuffer::clear_stencil(uint32_t s){
	for(size_t i_y = 0; i_y < height_; ++i_y){
		for(size_t i_x = 0; i_x < width_; ++i_x){
			sbuf_[i_y*width_ + i_x] = s;
		}
	}
}

void framebuffer::clear_color(size_t tar_id, const rect<size_t>& rc, const color_rgba32f& c){

	custom_assert(tar_id < cbufs_.size(), "��ȾĿ���ʶ���ô���");
	custom_assert(cbufs_[tar_id] && buf_valids[tar_id], "��ͼ��һ����Ч����ȾĿ��������ɫ��");
	custom_assert(rc.w + rc.x <= width_ && rc.h +rc.y <= height_, "�������򳬹���֡���巶Χ��");

	cbufs_[tar_id]->fill_clr_texels(rc.x, rc.y, rc.w, rc.h, c);
}

void framebuffer::clear_depth(const rect<size_t>& rc, float d){
	custom_assert(rc.w + rc.x <= width_ && rc.h +rc.y <= height_, "�������򳬹���֡���巶Χ��");

	for(size_t i_y = rc.y; i_y < rc.h + rc.y; ++i_y){
		for(size_t i_x = rc.x; i_x < rc.w + rc.x; ++i_x){
			dbuf_[i_y*width_ + i_x] = d;
		}
	}
}

void framebuffer::clear_stencil(const rect<size_t>& rc, uint32_t s){
	custom_assert(rc.w + rc.x <= width_ && rc.h +rc.y <= height_, "�������򳬹���֡���巶Χ��");

	for(size_t i_y = rc.y; i_y < rc.h + rc.y; ++i_y){
		for(size_t i_x = rc.x; i_x < rc.w + rc.x; ++i_x){
			sbuf_[i_y*width_ + i_x] = s;
		}
	}
}