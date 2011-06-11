#ifndef SOFTARTX_FREEIMAGE_UTILITIES_H
#define SOFTARTX_FREEIMAGE_UTILITIES_H

#include <salviax/include/utility/user_config.h>

#ifdef SOFTARTX_FREEIMAGE_ENABLED

#include "utility_forward.h"
#include "salviar/include/colors.h"
#include <eflib/include/string/string.h>
#include <eflib/include/math/collision_detection.h>
#include <FreeImage.h>
#include <boost/static_assert.hpp>
#include <algorithm>
#include <tchar.h>

BEGIN_NS_SOFTARTX_UTILITY()
// ��ȡλͼ
FIBITMAP* load_image(const std::_tstring& fname, int flag FI_DEFAULT(0));

// ���λͼ�Ƿ�Ϊ����װ����֧�ֵ�����
bool check_image_type_support(FIBITMAP* image);

// ��λͼ��ָ���������ſ�����ָ����С�ĸ�����
FIBITMAP* make_bitmap_copy( eflib::rect<size_t>& out_region,
						   size_t dest_width, size_t dest_height,
						   FIBITMAP* image, const eflib::rect<size_t>& src_region );

// ͳһ��ɫ���ֽ����з��ʡ�
// ����ͬ�ֽ����б�ʾ����ɫ��ͨ��������ת����ͳһ�ķ��ʽӿڡ�
template <class ColorType>
struct FREE_IMAGE_UNIFORM_COLOR
{
	STATIC_ASSERT_INFO(FIUC_RECIEVED_A_ILLEGAL_TYPE);
	BOOST_STATIC_ASSERT(FIUC_RECIEVED_A_ILLEGAL_TYPE);

	typedef int CompT;
	const CompT& r;
	const CompT& g;
	const CompT& b;
	const CompT& a;
	FREE_IMAGE_UNIFORM_COLOR(const ColorType& color);
private:
	FREE_IMAGE_UNIFORM_COLOR(const FREE_IMAGE_UNIFORM_COLOR<ColorType>&);
	FREE_IMAGE_UNIFORM_COLOR& operator = (const FREE_IMAGE_UNIFORM_COLOR<ColorType>&);
};

template <>
struct FREE_IMAGE_UNIFORM_COLOR<RGBQUAD>
{
	typedef byte CompT;
	const CompT& r;
	const CompT& g;
	const CompT& b;
	const CompT& a;
	FREE_IMAGE_UNIFORM_COLOR(const CompT* c, CompT /*alpha*/):r(c[FI_RGBA_RED]), g(c[FI_RGBA_GREEN]), b(c[FI_RGBA_BLUE]), a(c[FI_RGBA_ALPHA]){}
private:
	FREE_IMAGE_UNIFORM_COLOR(const FREE_IMAGE_UNIFORM_COLOR<RGBQUAD>&);
	FREE_IMAGE_UNIFORM_COLOR& operator = (const FREE_IMAGE_UNIFORM_COLOR<RGBQUAD>&);
};

template <>
struct FREE_IMAGE_UNIFORM_COLOR<RGBTRIPLE>
{
	typedef byte CompT;
	const CompT& r;
	const CompT& g;
	const CompT& b;
	const CompT& a;
	FREE_IMAGE_UNIFORM_COLOR(const CompT* c, CompT alpha):r(c[FI_RGBA_RED]), g(c[FI_RGBA_GREEN]), b(c[FI_RGBA_BLUE]), a(alpha){}

private:
	FREE_IMAGE_UNIFORM_COLOR(const FREE_IMAGE_UNIFORM_COLOR<RGBTRIPLE>&);
	FREE_IMAGE_UNIFORM_COLOR& operator = (const FREE_IMAGE_UNIFORM_COLOR<RGBTRIPLE>&);
};

template <>
struct FREE_IMAGE_UNIFORM_COLOR<FIRGBF>
{
	typedef float CompT;
	const CompT& r;
	const CompT& g;
	const CompT& b;
	const CompT& a;
	FREE_IMAGE_UNIFORM_COLOR(const CompT* c, CompT alpha):r(c[0]), g(c[1]), b(c[2]), a(alpha){}
private:
	FREE_IMAGE_UNIFORM_COLOR(const FREE_IMAGE_UNIFORM_COLOR<FIRGBF>&);
	FREE_IMAGE_UNIFORM_COLOR& operator = (const FREE_IMAGE_UNIFORM_COLOR<FIRGBF>&);
};

template <>
struct FREE_IMAGE_UNIFORM_COLOR<FIRGBAF>
{
	typedef float CompT;
	const CompT& r;
	const CompT& g;
	const CompT& b;
	const CompT& a;
	FREE_IMAGE_UNIFORM_COLOR(const CompT* c, CompT /*alpha*/):r(c[0]), g(c[1]), b(c[2]), a(c[3]){}
private:
	FREE_IMAGE_UNIFORM_COLOR(const FREE_IMAGE_UNIFORM_COLOR<FIRGBAF>&);
	FREE_IMAGE_UNIFORM_COLOR& operator = (const FREE_IMAGE_UNIFORM_COLOR<FIRGBAF>&);
};

#define FIUC FREE_IMAGE_UNIFORM_COLOR 

// �����FreeImage�ڲ���ɫ��ʽ��ͬ�ģ�������RGBAɫ�����е�SoftArt��ɫ���͡�
template<typename FIColorT>
struct softart_rgba_color_type{
	typedef softart::color_max type;
	static const softart::pixel_format fmt = softart::pixel_type_to_fmt<typename type>::fmt;
};
template<>
struct softart_rgba_color_type<RGBQUAD>{
	typedef softart::color_rgba8 type;
	static const softart::pixel_format fmt = softart::pixel_type_to_fmt<typename type>::fmt;
};

template<>
struct softart_rgba_color_type<RGBTRIPLE>{
	typedef softart::color_rgba8 type;
	static const softart::pixel_format fmt = softart::pixel_type_to_fmt<typename type>::fmt;
};

template<>
struct softart_rgba_color_type<FIRGBF>{
	typedef softart::color_rgba32f type;
	static const softart::pixel_format fmt = softart::pixel_type_to_fmt<typename type>::fmt;
};

template<>
struct softart_rgba_color_type<FIRGBAF>{
	typedef softart::color_rgba32f type;
	static const softart::pixel_format fmt = softart::pixel_type_to_fmt<typename type>::fmt;
};
END_NS_SOFTARTX_UTILITY()

#endif // SOFTART_FREEIMAGE_ENABLED

#endif