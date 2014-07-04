#pragma once

#include <boost/shared_ptr.hpp>

class display_mgr;

class kmd_adapter
{
public:
	NTSTATUS create(const boost::shared_ptr<display_mgr>& disp_mgr, D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME* adapter_from_name);
	NTSTATUS create(D3DKMT_HANDLE* adapter, LUID* adapter_luid);

	NTSTATUS query_adapter_info(const D3DKMT_QUERYADAPTERINFO* query_ai);
	NTSTATUS create_device(D3DKMT_CREATEDEVICE* cd);

private:
	D3DKMT_HANDLE adapter_;
};
