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

#ifdef SOFTARTX_D3D9_ENABLED
#include "softartx/include/utility/d3d9_utilities.h"
BEGIN_NS_SOFTARTX_UTILITY()
d3d9_device::d3d9_device(const d3d9_device_param& param, D3DPRESENT_PARAMETERS& present_params){
	//��ʼ��D3D9
	if(pd3d9_ == NULL){
		pd3d9_ = Direct3DCreate9(D3D_SDK_VERSION);
	} else {
		pd3d9_->AddRef();
	}

	//��ʼ��Device
	if( FAILED(pd3d9_->CreateDevice(
		param.adapter,	param.devtype,	param.focuswnd, param.behavior, 
		&present_params, &pd3ddev9_
		))) {
		pd3ddev9_ = NULL;
	}
}

d3d9_device::~d3d9_device(){
	if(pd3ddev9_){
		pd3ddev9_->Release();
	}
	if(pd3d9_){
		pd3d9_->Release();
	}
}

h_d3d9_device d3d9_device::create(const d3d9_device_param &param, D3DPRESENT_PARAMETERS &present_params){
	return h_d3d9_device( new d3d9_device( param, present_params ) );
}

IDirect3D9* d3d9_device::get_d3d9() const{
	return pd3d9_;
}

IDirect3DDevice9* d3d9_device::get_d3d_device9() const{
	return pd3ddev9_;
}

END_NS_SOFTARTX_UTILITY()

#endif //SOFTARTX_D3D9_ENABLED