#if 0

#include <sasl/include/semantic/reflector.h>

#include <sasl/include/semantic/semantics.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/syntax_tree/declaration.h>

#include <sasl/enums/builtin_types.h>
#include <sasl/enums/enums_utility.h>

#include <salviar/include/rfile.h>

#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/foreach.hpp>
#include <boost/utility/addressof.hpp>
#include <eflib/include/platform/boost_end.h>

#include <algorithm>

using namespace sasl::syntax_tree;
using namespace sasl::utility;
using namespace salviar;

using eflib::fixed_string;

using boost::addressof;
using boost::make_shared;
using boost::shared_ptr;

using std::lower_bound;
using std::string;
using std::vector;
using std::make_pair;

BEGIN_NS_SASL_SEMANTIC();

EFLIB_DECLARE_CLASS_SHARED_PTR(reflector);
EFLIB_DECLARE_CLASS_SHARED_PTR(reflection_impl);
class rfile_impl;

static const size_t REGISTER_SIZE = 16;

enum class alloc_result: uint32_t
{
	ok = 0,
	out_of_reg_bound,
	register_has_been_allocated
};

class struct_layout
{
public:
	struct_layout();

	size_t add_member(size_t sz)
	{
		size_t offset = 0;
		if(total_size_ % 16 == 0)
		{
			offset = total_size_;
		}
		else if ( total_size_ + sz >= eflib::round_up(total_size_, REGISTER_SIZE) )
		{
			offset = eflib::round_up(total_size_, 16);
		}

		total_size_ = offset + sz;
		return offset;
	}

	size_t size() const
	{
		return eflib::round_up(total_size_, 16);
	}

private:
	size_t total_size_;
};

struct reg_handle
{
	rfile_impl*	rfile;
	size_t		v;
};

class rfile_impl: public reg_file
{
private:
	typedef boost::icl::interval_map<reg_name, uint32_t /*physical_reg*/>
		reg_addr_map;
	typedef reg_addr_map::interval_type	reg_interval;

public:
	rfile_impl(rfile_categories cat, uint32_t index)
		: uid_(cat, index), total_reg_count_(0)
	{
	}

	reg_name absolute_reg(reg_name const& rname) const
	{
		if(rname.rfile.cat == uid_.cat || rname.rfile.cat == rfile_categories::offset)
		{
			return reg_name(uid_, rname.reg_index, rname.elem);
		}
		return reg_name();
	}

	alloc_result alloc_reg(size_t sz, reg_name const& rname)
	{	
		reg_name reg_end;
		return alloc_reg(reg_end, rname, sz);
	}

	reg_handle auto_alloc_reg(size_t sz)
	{
		auto_alloc_sizes_.push_back( static_cast<uint32_t>(sz) );
		reg_handle ret;
		ret.v = auto_alloc_sizes_.size() - 1;
		return ret;
	}

	void update_auto_alloc_regs()
	{
		for(auto sz: auto_alloc_sizes_)
		{
			reg_name beg, end;
			alloc_auto_reg(beg, end, sz);
			auto_alloc_regs_.push_back(beg);
		}
	}

	void assign_semantic(reg_name const& beg, size_t sz, semantic_value const& sv_beg)
	{
		reg_sv_[beg] = sv_beg;

		if(sz > REGISTER_SIZE)
		{
			size_t reg_count = sz / REGISTER_SIZE;
			for(size_t reg_dist = 1; reg_dist < reg_count; ++reg_dist)
			{
				auto rname = beg.advance(reg_dist);
				reg_sv_[rname] = sv_beg.advance_index(reg_dist);
			}
		}
	}

	reg_name find_reg(semantic_value const& sv) const
	{
		auto iter = sv_reg_.find(sv);
		if(iter == sv_reg_.end())
		{
			return reg_name();
		}
		return iter->second;
	}
	reg_name find_reg(reg_handle const& rh) const
	{
		return auto_alloc_regs_[rh.v];
	}

	void update_addr()
	{
		total_reg_count_ = 0;
		for(auto& range_addr: used_regs_)
		{
			auto& reg_range = range_addr.first;
			uint32_t reg_count = reg_range.upper().reg_index - reg_range.lower().reg_index;
			range_addr.second = total_reg_count_;
			total_reg_count_ += reg_count;
		}
	}

	uint32_t used_reg_count() const
	{
		return total_reg_count_;
	}

	uint32_t reg_addr(reg_name const& rname) const
	{
		auto iter = used_regs_.find(rname);
		if( iter == used_regs_.end() )
		{
			return static_cast<uint32_t>(-1);
		}
		return iter->second + (rname.reg_index - iter->first.lower().reg_index);
	}

private:
	alloc_result alloc_auto_reg(reg_name& beg, reg_name& end, size_t sz)
	{
		auto& slot_used_regs = used_regs_;
		beg = slot_used_regs.empty() ? reg_name(uid_, 0, 0) : slot_used_regs.rbegin()->first.upper().advance(1);
		return alloc_reg(end, beg, sz);
	}

	alloc_result alloc_reg(reg_name& end, reg_name const& beg, size_t sz)
	{
		if( 4 - beg.elem < sz / 4 )
		{
			return alloc_result::out_of_reg_bound;
		}
		end = beg.advance( eflib::round_up(sz, 16) / 16 );
		used_regs_.add( std::make_pair(reg_interval::right_open(beg, end), 0) );
		return alloc_result::ok;
	}

	reg_file_uid						uid_;
	uint32_t							total_reg_count_;

	reg_addr_map						used_regs_;
	std::map<node_semantic*, reg_name>	var_regs_;
	std::vector<uint32_t /*reg size*/>	auto_alloc_sizes_;
	std::vector<reg_name>				auto_alloc_regs_;

	std::map<semantic_value, reg_name>	sv_reg_;
	std::map<reg_name, semantic_value>	reg_sv_;
	std::map<uint32_t /*reg index*/, uint32_t /*addr*/>
		reg_addr_;
};

class reflection_impl
{
public:
	// Friend for reflector could call compute_layout();
	friend class reflector;

	virtual eflib::fixed_string	entry_name() const;

	void	initialize(languages prof);

	rfile_impl* rfile(rfile_categories cat, uint32_t rfile_index)
	{
		auto& cat_rfiles = rfiles_[static_cast<uint32_t>(cat)];
		return &cat_rfiles[rfile_index];
	}

	rfile_impl* rfile(reg_file_uid rfile_id)
	{
		return rfile(rfile_id.cat, rfile_id.index);
	}

	void add_variable_reg(node_semantic const* var, reg_name const& rname)
	{
		input_var_regs_.insert( make_pair(var, rname) );
	}

	void add_variable_reg(node_semantic const* var, reg_handle const& rhandle)
	{
		var_auto_regs_.push_back( make_pair(var, rhandle) );
	}

	void assign_semantic(reg_name const& rname, size_t sz, semantic_value const& sv)
	{
		rfile(rname.rfile)->assign_semantic(rname, sz, sv);
	}

	void update_auto_alloc_regs()
	{
		for(auto& cat_rfiles: rfiles_)
		{
			for(auto& rfile: cat_rfiles)
			{
				rfile.update_auto_alloc_regs();
			}
		}

		for(auto& var_reg: var_auto_regs_)
		{
			add_variable_reg( var_reg.first, var_reg.second.rfile->find_reg(var_reg.second) );
		}

		var_auto_regs_.clear();
	}

	void update_reg_address()
	{
		for(uint32_t cat = static_cast<uint32_t>(rfile_categories::unknown); cat = static_cast<uint32_t>(rfile_categories::count); ++cat)
		{
			auto& rfile			= rfiles_[cat];
			auto& rfiles_addr	= rfile_start_addr_[cat];

			uint32_t total = 0;
			for(auto& rfile: rfile)
			{
				rfiles_addr.push_back(total);
				total += rfile.used_reg_count();
			}
			used_reg_count_[cat] = total;
		}
	}

	uint32_t used_reg_count(rfile_categories cat)
	{
		return used_reg_count_[static_cast<uint32_t>(cat)];
	}

	reg_name find_reg(reg_handle rhandle) const
	{
		return rhandle.rfile->find_reg(rhandle);
	}

	uint32_t reg_addr(reg_name const& rname)
	{
		uint32_t cat = static_cast<uint32_t>(rname.rfile.cat);
		return
			rfile_start_addr_[cat][rname.rfile.index] +
			rfiles_[cat][rname.rfile.index].reg_addr(rname);
	}

	// Impl specific members
	reflection_impl();

	void module( module_semantic_ptr const& );
	bool is_module( module_semantic_ptr const& ) const;

	void entry(symbol*);
	bool is_entry(symbol*) const;

private:
	module_semantic*	module_sem_;

	symbol*				entry_point_;
	eflib::fixed_string	entry_point_name_;

	std::vector<rfile_impl>
		rfiles_				[static_cast<uint32_t>(rfile_categories::count)];
	uint32_t			used_reg_count_		[static_cast<uint32_t>(rfile_categories::count)];
	std::vector<uint32_t>
		rfile_start_addr_	[static_cast<uint32_t>(rfile_categories::count)];

	std::vector< std::pair<node_semantic const*, reg_handle> >
		var_auto_regs_;

	boost::unordered_map<node_semantic const*, reg_name>
		input_var_regs_;
};

class reflector
{
public:
	reflector(module_semantic* sem, eflib::fixed_string const& entry_name)
		: sem_(sem), current_entry_(NULL), reflection_(NULL), entry_name_(entry_name)
	{
	}

	reflector(module_semantic* sem)	: sem_(sem), current_entry_(NULL), reflection_(NULL)
	{
	}

	reflection_impl_ptr reflect()
	{
		if( !entry_name_.empty() )
		{
			vector<symbol*> overloads = sem_->root_symbol()->find_overloads(entry_name_);
			if ( overloads.size() != 1 )
			{
				return reflection_impl_ptr();
			}
			current_entry_ = overloads[0];
			return do_reflect();
		}
		else
		{
			symbol*				candidate = NULL;
			reflection_impl_ptr	candidate_reflection;
			BOOST_FOREACH( symbol* fn_sym, sem_->functions() )
			{
				current_entry_ = fn_sym;
				candidate_reflection = do_reflect();

				if(candidate_reflection)
				{
					if(candidate)
					{
						// TODO: More than one matched. conflict error.
						return reflection_impl_ptr();
					}
					candidate = fn_sym;
				}
			}
			return candidate_reflection;
		}
	}

private:
	reflection_impl_ptr do_reflect()
	{
		if( ! (sem_ && current_entry_) )
		{
			return reflection_impl_ptr();
		}

		salviar::languages lang = sem_->get_language();

		// Initialize language ABI information.
		reflection_impl_ptr ret = make_shared<reflection_impl>();
		ret->module_sem_		= sem_;
		ret->entry_point_		= current_entry_;
		ret->entry_point_name_	= current_entry_->mangled_name();
		reflection_				= ret.get();

		if( lang == salviar::lang_vertex_shader
			|| lang == salviar::lang_pixel_shader
			|| lang == salviar::lang_blending_shader
			)
		{
			// Process entry function.
			shared_ptr<function_def> entry_fn
				= current_entry_->associated_node()->as_handle<function_def>();
			assert(entry_fn);

			// 3 things to do:
			//		1. struct member offset computation;
			//		2. register allocation;
			//		3. semantic allocation;

			// Process parameters
			for(auto& param: entry_fn->params)
			{
				if( !process_entry_inputs(nullptr, param, false) )
				{
					ret.reset();
					return ret;
				}
			}

			// Process global variables.
			for(symbol* gvar_sym: sem_->global_vars())
			{
				auto gvar = gvar_sym->associated_node()->as_handle<declarator>();
				if( !process_entry_inputs(nullptr, gvar, true) )
				{
					//TODO: It an semantic error need to be reported.
					ret.reset();
					return ret;
				}
			}

			process_auto_alloc_regs();
			assign_semantics();
		}

		return ret;
	}

	struct variable_info
	{
		size_t			offset;
		size_t			size;
		semantic_value	sv;					// only available for leaf.
		
		reg_name		reg;
		reg_handle		rhandle;

		variable_info*	parent;
		variable_info*	first_child;
		variable_info*	last_child;
		variable_info*	sibling;

		void initialize()
		{
			offset = 0;
			size = 0;
			parent = first_child = last_child = sibling = nullptr;
		}
	};

	bool process_entry_inputs(
		variable_info*		parent_info,		// in and out
		node_ptr const&		v,
		bool				is_global
		)
	{
		bool const is_member = (parent_info == nullptr);
		var_infos_.push_back( variable_info() );
		variable_info* minfo  = &var_infos_.back();
		minfo->initialize();
		minfo->parent = parent_info;

		assert(reflection_);
		node_semantic*	node_sem	= sem_->get_semantic( v.get() );
		tynode*			ty			= node_sem->ty_proto();

		rfile_impl* rfile =
			reflection_->rfile(
			is_global
			? reg_file_uid::global() 
			: ty->is_uniform() ? reg_file_uid::params() : reg_file_uid::varyings()
			);

		bool		use_parent_sv	= false;
		auto const* node_sv			= node_sem->semantic_value();
		auto		user_reg		= rfile->absolute_reg( node_sem->user_defined_reg() );

		// Member-special check: for register and semantic overwrite
		if(is_member)
		{
			if( ty->is_uniform() )
			{
				// TODO: error function parameters cannot be declared 'uniform'
				return false;
			}

			if ( node_sem->user_defined_reg().valid() )
			{
				// TODO: structure member cannot specify position
				return false;
			}

			if ( node_sv->valid() )
			{
				if( parent_info->sv.valid() )
				{
					// TODO: error: semantic rebounded
					return false;
				}
			}
			else
			{
				use_parent_sv = true;
				node_sv = &parent_info->sv;
			}
		}

		// For builtin type
		if( ty->is_builtin() )
		{
			// no-semantic bounded varying variables check
			if(node_sv == nullptr && !is_global)
			{
				// TODO: error: no semantic bound on varying.
				return false;
			}

			minfo->size = reg_storage_size(ty->tycode);

			size_t reg_count = eflib::round_up(minfo->size, REGISTER_SIZE) / 16;
			if(is_member)
			{
				minfo->sv = *node_sv;
				if(use_parent_sv)
				{
					parent_info->sv = node_sv->advance_index(reg_count);
				}
			}
		}

		// For structure
		if( ty->node_class() == node_ids::struct_type )
		{
			// TODO: statistic total size, and member offset of structure via 'struct_layout'
			struct_type*	struct_ty = dynamic_cast<struct_type*>(ty);
			struct_layout	layout;
			for(auto const& decl: struct_ty->decls)
			{
				if ( decl->node_class() != node_ids::variable_declaration )
				{
					continue;
				}

				auto vardecl = decl->as_handle<variable_declaration>();
				for(auto const& dclr: vardecl->declarators)
				{
					process_entry_inputs(minfo, dclr, is_global);
					size_t offset = layout.add_member(minfo->last_child->size);
					minfo->last_child->offset = offset;
					sem_->get_semantic(dclr)->member_offset(offset);
				}
			}

			minfo->size = layout.size();
			if(use_parent_sv)
			{
				parent_info->sv = minfo->sv;
			}
		}

		if(parent_info)
		{  
			if(!parent_info->first_child)
			{
				parent_info->first_child = parent_info->last_child = minfo;
			}
			else
			{
				parent_info->last_child->sibling = minfo;
				parent_info->last_child = minfo;
			}
		}
		else
		{
			// Note: Register allocation only for top-level variable.
			if( user_reg.valid() )
			{
				if( rfile->alloc_reg(minfo->size, user_reg) != alloc_result::ok)
				{
					// TODO: error: register allocation failed. Maybe out of capacity or register has been allocated.
					return false;
				}
				reflection_->add_variable_reg(node_sem, user_reg);
				minfo->reg = user_reg;
			}
			else
			{
				auto rhandle = rfile->auto_alloc_reg(minfo->size);
				reflection_->add_variable_reg(node_sem, rhandle);
				minfo->rhandle = rhandle;
			}
		}

		return true;
	}

	void process_auto_alloc_regs()
	{
		reflection_->update_auto_alloc_regs();
	}

	void assign_semantics()
	{
		for(auto& var_info: var_infos_)
		{
			// Compute reg.
			if( var_info.rhandle.rfile != nullptr )
			{
				var_info.reg = reflection_->find_reg(var_info.rhandle);
			}

			if( !var_info.reg.valid() )
			{
				var_info.reg = var_info.parent->reg.advance(var_info.offset);
			}

			// Semantics only apply leaf nodes.
			if(var_info.first_child == nullptr && var_info.sv.valid())
			{
				reflection_->assign_semantic(var_info.reg, var_info.size, var_info.sv);
			}
		}
	}

	std::deque<variable_info>
						var_infos_;
	module_semantic*	sem_;
	fixed_string		entry_name_;
	symbol*				current_entry_;
	reflection_impl*	reflection_;
};

reflection_impl_ptr reflect(module_semantic_ptr const& sem)
{
	reflector rfl( sem.get() );
	return rfl.reflect();
}

reflection_impl_ptr reflect(module_semantic_ptr const& sem, eflib::fixed_string const& entry_name)
{
	reflector rfl( sem.get(), entry_name );
	return rfl.reflect();
}

END_NS_SASL_SEMANTIC();

#endif
