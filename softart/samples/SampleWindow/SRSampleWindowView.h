// SRSampleWindowView.h : interface of the CSRSampleWindowView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "softartx/include/presenter/dx9/dev_d3d9.h"
#include "softartx/include/presenter/gdiplus/dev_gdiplus.h"
#include "softartx/include/resource/mesh/sa/mesh_io.h"
#include "softart/include/shader.h"
#include "softart/include/renderer_impl.h"
#include "softart/include/resource_manager.h"
#include "softart/include/rasterizer.h"
#include "eflib/include/eflib.h"
#include <iostream>
#include <boost/assign.hpp>
#include <boost/timer.hpp>

//#define USE_GDIPLUS

using namespace efl;
using namespace boost;
using namespace boost::assign;
using namespace std;
using namespace softartx;
using namespace softartx::resource;
using namespace softartx::presenter;
using namespace Gdiplus;

struct vert
{
	vec4 pos;
};

class vs : public vertex_shader
{
	mat44 wvp;
public:
	vs():wvp(mat44::identity()){
		register_var(_T("WorldViewProjMat"), wvp);
	}

	vs(const mat44& wvp):wvp(wvp){}
	void shader_prog(const vs_input& in, vs_output& out)
	{
		vec4 pos = in[0];
		transform(out.position, wvp, pos);
		out.attributes[0] = in[0];//(vec4(1.0f, 1.0f, 1.0f, 1.0f) - in[0]);
		out.attribute_modifiers[0] = 0;
		out.num_used_attribute = 1;
	}
};

class ps : public pixel_shader
{
public:
	bool shader_prog(const vs_output& in, ps_output& out)
	{
		out.color[0].xyz(in.attributes[0].xyz());
		out.color[0].w = 1.0f;

		return true;
	}
};

class ts : public blend_shader
{
public:
	bool shader_prog(backbuffer_pixel_out& inout, const ps_output& in){
		if(inout.depth() > in.depth){
			inout.color(0, in.color[0]);
			inout.depth(in.depth);
		}
		return true;
	}
};

class CSRSampleWindowView : public CWindowImpl<CSRSampleWindowView>
{
public:
	h_device present_dev;
	h_renderer hsr;
	h_texture sm_tex;

	h_mesh planar_mesh;
	h_mesh box_mesh;

	uint32_t num_frames;
	float accumulate_time;
	float fps;

	boost::timer timer;

	CSRSampleWindowView::CSRSampleWindowView()
	{
	}

	CSRSampleWindowView::~CSRSampleWindowView()
	{
	}

	DECLARE_WND_CLASS(NULL)

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		pMsg;
		return FALSE;
	}

	BEGIN_MSG_MAP(CSRSampleWindowView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnClick)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
#ifdef USE_GDIPLUS
		present_dev = dev_gdiplus::create_device( NULL, Rect() );
#else
		D3DPRESENT_PARAMETERS d3dpp;
		std::memset(&d3dpp, 0, sizeof(d3dpp));
		d3dpp.Windowed					= true;
		d3dpp.BackBufferCount			= 1;
		d3dpp.BackBufferWidth			= 512;
		d3dpp.BackBufferHeight			= 512;
		d3dpp.hDeviceWindow				= m_hWnd;
		d3dpp.SwapEffect				= D3DSWAPEFFECT_DISCARD;
		d3dpp.PresentationInterval		= D3DPRESENT_INTERVAL_IMMEDIATE;
		d3dpp.BackBufferFormat			= D3DFMT_X8R8G8B8;

		softartx::utility::d3d9_device_param device_params;
		device_params.adapter = 0;
		device_params.devtype = D3DDEVTYPE_HAL;
		device_params.focuswnd = m_hWnd;
		device_params.behavior = D3DCREATE_HARDWARE_VERTEXPROCESSING;

		softartx::utility::h_d3d9_device d3d9dev = softartx::utility::d3d9_device::create(device_params, d3dpp);

		present_dev = dev_d3d9::create_device( d3d9dev );
#endif

		renderer_parameters render_params = {0};
		render_params.backbuffer_format = pixel_format_color_bgra8;
		render_params.backbuffer_height = 512;
		render_params.backbuffer_width = 512;

		hsr = create_software_renderer(&render_params, present_dev);

		present_dev->attach_framebuffer( hsr->get_framebuffer().get() );

		planar_mesh = create_planar(
			hsr.get(), 
			vec3(-3.0f, -1.0f, -3.0f), 
			vec3(6.0f, 0.0f, 0.0f), 
			vec3(0.0f, 0.0f, 6.0f),
			1, 1, true
			);
		
		box_mesh = create_box(hsr.get());

		num_frames = 0;
		accumulate_time = 0;
		fps = 0;

		return 0;
	}

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CPaintDC dc(m_hWnd);
		//TODO: Add your drawing code here

#ifdef USE_GDIPLUS
		RECT rc;
		this->GetClientRect(&rc);
		Graphics g(dc.m_hDC);
		Rect draw_rect((INT)rc.left, (INT)rc.top, hsr->get_framebuffer()->get_width(), hsr->get_framebuffer()->get_height());
		boost::shared_polymorphic_cast<dev_gdiplus>(present_dev)->set_device_info(&g, draw_rect);
#endif
		present_dev->present();
		return 0;
	}

	void Render()
	{
		// measure statistics
		++ num_frames;
		accumulate_time += static_cast<float>(timer.elapsed());

		// check if new second
		if (accumulate_time > 1)
		{
			// new second - not 100% precise
			fps = num_frames / accumulate_time;

			accumulate_time = 0;
			num_frames  = 0;
		}

		timer.restart();

		cout << fps << endl;

		hsr->clear_color(0, color_rgba32f(0.2f, 0.2f, 0.5f, 1.0f));
		hsr->clear_depth(1.0f);

		vs* pvs = new vs();
		ps* pps = new ps();
		ts* pts = new ts();

		vec4 camera(1.5f, 1.5f, 1.5f, 1.0f);

		mat44 world(mat44::identity()), view, proj, wvp;
		
		mat_lookat(view, camera, vec4::gen_coord(0.0f, 0.0f, 0.0f), vec4::gen_vector(0.0f, 1.0f, 0.0f));
		mat_perspective_fov(proj, float(PI) / 2.0f, 1.0f, 0.1f, 100.0f);
		mat_mul(wvp, mat_mul(wvp, proj, view), world);

		pvs->set_constant(_T("WorldViewProjMat"), &wvp);

		hsr->set_vertex_shader(h_vertex_shader(pvs));
		hsr->set_pixel_shader(h_pixel_shader(pps));
		hsr->set_blend_shader(h_blend_shader(pts));

		hsr->set_cull_mode(cull_none);

		box_mesh->render();

		InvalidateRect(NULL);
	}

	LRESULT OnClick(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
	{
		framebuffer* pfb = static_pointer_cast<renderer_impl>(hsr)->get_framebuffer().get();
		PPOINTS pp = (PPOINTS)(&lParam);
		if(pfb && size_t(pp->x) < pfb->get_width() && size_t(pp->y) < pfb->get_height())
		{
			color_rgba32f c = pfb->get_render_target(render_target_color, 0)->get_texel(pp->x, pfb->get_height() - 1 - pp->y);
			TCHAR str[512];
			_stprintf(str, _T("Pos: %3d, %3d, Color: %8.6f,%8.6f,%8.6f"), pp->x, pp->y, c.r, c.g, c.b);
			this->GetParent().SetWindowText(str);
		}
		return 0;
	}

	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		UNREF_PARAM(uMsg);
		UNREF_PARAM(wParam);
		UNREF_PARAM(lParam);
		UNREF_PARAM(bHandled);

		return 1;
    }
};
