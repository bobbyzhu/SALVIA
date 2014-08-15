#include <salviau/include/common/sample_app.h>

BEGIN_NS_SALVIAU();
#if 0
sample_app::sample_app(std::string const& app_name, std::string const& options)
{
}

sample_app::~sample_app()
{
}

void sample_app::run()
{
	switch(mode_)
	{
	case app_modes::interactive:
		break;
	case app_modes::benchmark:
		break;
	case app_modes::replay:
		break;
	case app_modes::test:
		break;
	}

	// parsing mode and params
	on_create();

	while(!exiting_)
	{
		tm_.update();
		on_frame( frame_++, tm_.elapsed() );
	}
}
	
// Utilities
void sample_app::profiling(std::string const& stage_name, std::function<void()> const& fn)
{
}

void sample_app::save_frame(salvia::surface_ptr const& surf)
{
}

void sample_app::save_profiling_result(std::string const& file_name)
{
}

size_t sample_app::total_frames() const
{
	return frame_;
}
#endif
END_NS_SALVIAU();
