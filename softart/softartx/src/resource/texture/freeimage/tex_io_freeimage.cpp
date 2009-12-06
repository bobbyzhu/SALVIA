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

#ifdef SOFTARTX_FREEIMAGE_ENABLED

#include "softartx/include/utility/freeimage_utilities.h"
#include "softartx/include/resource/texture/freeimage/tex_io_freeimage.h"
#include "softart/include/renderer_impl.h"
#include "softart/include/resource_manager.h"
#include "eflib/include/eflib.h"
#include <FreeImage.h>
#include <tchar.h>
#include <boost/static_assert.hpp>
#include <algorithm>

using namespace efl;
using namespace std;

BEGIN_NS_SOFTARTX_RESOURCE()

//��FIBITMAP��ͼ�񿽱���Surface�С�
//	��������Ϊ���¼�����
//		���Ƚ�FIBITMAP�е�������ɫ������������FIBITMAP�еķ�����Ϣ��
//		���ݷ�����Ϣ������һ��RGBAɫ���SoftArt���м���ɫ������FIBITMAP���ص����ֽ��򿽱����м���ɫ��
//		��SoftArt�м���ɫ�任��Ŀ��Surface����ɫ��ʽ��������Ŀ��Surface�С�
//	ģ�������FIColorT��FIBITMAP�Ĵ洢��ʽ
template<typename FIColorT> bool copy_image_to_surface(
	surface& surf, const rect<size_t>& dest_rect,
	FIBITMAP* image, const rect<size_t>& src_rect,
	typename FIUC<FIColorT>::CompT default_alpha = (typename FIUC<FIColorT>::CompT)(0) 
	)
{
	if (image == NULL){
		return false;
	}

	if ( src_rect.h != dest_rect.h || src_rect.w != dest_rect.w ){
		return false;
	}

	byte* pdata = NULL;
	surf.lock((void**)&pdata, dest_rect, lock_write_only);
	if(!pdata) { return false;}

	BYTE* image_data = FreeImage_GetBits(image);
	size_t pitch = FreeImage_GetPitch(image);
	size_t bpp = (FreeImage_GetBPP(image) >> 3);

	image_data = (pitch * src_rect.y) + bpp * src_rect.x;

	for(size_t iheight = 0; iheight < src_rect.h; ++iheight)
	{
		byte* ppixel = image_data;
		for(size_t iwidth = 0; iwidth < src_rect.w; ++iwidth)
		{
			FIUC<FIColorT> uc((typename FIUC<FIColorT>::CompT*)ppixel, alpha);
			typename softart_rgba_color_type<FIColorT>::type c(uc.r, uc.g, uc.b, uc.a);

			pixel_format_convertor::convert(
				surf.get_pixel_format(),
				softart_rgba_color_type<FIColorT>::fmt, 
				pdata,
				&c);

			ppixel += bpp;
			pdata += color_infos[surf.get_pixel_format()].size;
		}
		image_data += pitch;
	}

	surf.unlock();
	return true;
}
//��Image�ľֲ�������surface���ָ�������ڡ����Դ�����Ŀ�������С��ͬ�������˫�߲�ֵ�����š�
bool texture_io_fi::load( surface& surf, const rect<size_t>& dest_region, FIBITMAP* img, const rect<size_t>& src_region ){
	rect<size_t> scaled_img_region ;
	FIBITMAP* scaled_img = make_image_copy(scaled_img_region, dest_region.w, dest_region.h, img, src_region);

	if(image_type == FIT_RGBAF){
		if(! copy_image_to_surface<FIRGBAF>(surf, 0, scaled_image, scaled_img_region) ){
			is_success = false;
		}
	}
	if(image_type == FIT_BITMAP)
	{
		if(FreeImage_GetColorType(scaled_image) == FIC_RGBALPHA){
			if(! copy_image_to_texture<RGBQUAD>(
				surf, 0, scaled_image, scaled_img_region)){
				is_success = false;
			}
		} else {
			if(! copy_image_to_surface<RGBTRIPLE>(
				surf, 0, scaled_image, scaled_img_region, 255)){
				is_success = false;
			}
		}
	}

	FreeImage_Unload(scaled_img);
}
//����ͼ�񴴽�����
h_texture texture_io_fi::load(renderer* pr, const std::_tstring& filename, pixel_format tex_pxfmt){
	FIBITMAP* img = load_image( filename );
	
	size_t src_w = FreeImage_GetWidth(img);
	size_t src_h = FreeImage_GetHeight(img);

	return load(pr, img, rect<size_t>(0, 0, src_w, src_h), tex_pxfmt, src_w, src_h);
}

//ѡȡͼ���һ���ִ�������
h_texture texture_io_fi::load(renderer* pr,
		const FIBITMAP* img, const efl::rect<size_t>& src,
		pixel_format tex_pxfmt, size_t dest_width, size_t dest_height)
{
	h_texture ret((texture*)NULL);
	ret = psr->get_tex_mgr()->create_texture_2d(src.w, src.h, tex_pxfmt);
	if( !load(ret->get_surface(0), filename, tex_pxfmt) ){
		ret.reset();
	}
	return ret;
}

//ʹ������ͼ�񴴽�Cube����Cube�����ÿ���С�͵�һ������Ĵ�С��ͬ����������ļ��Ĵ�С���һ�Ų�ͬ���򰴵�һ�ŵĴ�С���š�
h_texture texture_io_fi::load_cube(renderer* pr, const vector<_tstring>& filenames, pixel_format fmt){
	h_texture ret(NULL);

	for(int i_cubeface = 0; i_cubeface < 6; ++i){
		FIBITMAP* cube_img = load_image( filenames[i_cubeface] );
		if (cube_img == NULL){
			ret.reset();
			return ret;
		}

		//��һ�ε��õ�ʱ�򴴽�cube
		if ( !ret ){
			size_t img_w = FreeImage_GetWidth(cube_img);
			size_t img_h = FreeImage_GetHeight(cube_img);

			ret = pr->get_tex_mgr()->create_texture_cube( img_w, img_h, fmt );
		}

		texture_cube* ptexcube = (texture_cube*)(ret.get());
		texture& face_tex = ptexcube->get_face(cubemap_faces(i));
		rect<size_t> copy_region(0, 0, face_tex.get_width(), face_tex.get_height());
		load( face_tex.get_surface(0), copy_region, cube_img, copy_region );
	}

	return ret;
}
//�����水��PNG����HDR��ʽ����Ϊ�ļ���
void texture_io_fi::save(const surface& surf, const std::_tstring& filename, pixel_format pxfmt){
	FREE_IMAGE_TYPE fit = FIT_UNKNOWN;
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;

	FIBITMAP* image = NULL;

	switch(format){
case pixel_format_color_bgra8:
	fit = FIT_BITMAP;
	fif = FIF_PNG;
	image = FreeImage_AllocateT(fit, int(surf.get_width()), int(surf.get_height()), 32, 0x0000FF, 0x00FF00, 0xFF0000);
	break;
case pixel_format_color_rgb32f:
	fit = FIT_RGBF;
	fif = FIF_HDR;
	image = FreeImage_AllocateT(fit, int(surf.get_width()), int(surf.get_height()), 96);
	break;
default:
	custom_assert(false, "�ݲ�֧�ָø�ʽ��");
	return;
	}

	byte* psurfdata = NULL;
	surf.lock((void**)&psurfdata, rect<size_t>(0, 0, surf.get_width(), surf.get_height()), lock_read_only);

	byte* pimagedata = FreeImage_GetBits(image);

	for(size_t ih = 0; ih < surf.get_height(); ++ih){
		pixel_format_convertor::convert_array(
			format,
			surf.get_pixel_format(),
			pimagedata, psurfdata,
			int(surf.get_width())
			);
		psurfdata += color_infos[surf.get_pixel_format()].size * surf.get_width();
		pimagedata += FreeImage_GetPitch(image);
	}

	surf.unlock();

	FreeImage_Save(fif, image, to_ansi_string(filename).c_str());
	FreeImage_Unload(image);
}
END_NS_SOFTARTX_RESOURCE()

#endif