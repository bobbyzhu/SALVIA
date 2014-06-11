template <typename T>
T round_up(T v, T align)
{
	return (v + align - 1) / align * align;
}

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
		else if ( total_size_ + sz >= round_up(total_size_, 16) )
		{
			offset = round_up(total_size_, 16);
		}
		
		total_size_ = offset + sz;
	}
	
	size_t size() const
	{
		return round_up(total_size_, 16);
	}
	
private:
	size_t total_size_;
};

enum class reg_slots: uint32_t
{
	unknown = 0,
	uniforms,				// cb#, s#, t#
	varying,				// v#
	outputs,				// o#
	count
};

uint32_t SLOT_BUFFER_COUNTS[] = 
{
	0,
	16 + 16 + 2 + 2,	// CB(16) + ICB(16) + Samplers + Textures + Global + Params
	1,					// Varying
	1
}

enum class reg_types
{
	none = 0,
	texture,
	sampler,
	uav,
	cbuffer,
	output,
	count
};

struct buffer_uid
{
	buffer_uid()
		:slot(reg_slots::unknown), index(0)
	{
	}
	
	buffer_uid(reg_slots slot, uint32_t index)
		:slot(slot), index(index);
	{
	}
	
	buffer_uid(buffer_uid const& rhs)
		:slot(rhs.slot), index(rhs.index)
	{
	}
	
	buffer_uid& operator = (buffer_uid const& rhs)
	{
		slot = rhs.slot;
		index = rhs.index;
		return *this;
	}
	
	reg_slots slot;
	size_t	  index;
};

struct reg_name
{
	buffer_uid	buf;
	uint32_t	reg_index;
	uint32_t	elem;
	
	reg_name(): reg_index(0), elem(0)
	{
	}
	
	reg_name(reg_slots slot, uint32_t buf_index, uint32_t reg_index, uint32_t elem)
		: buf(slot, buf_index), reg_index(reg_index), elem(elem)
	{
	}
	
	reg_name(buffer_uid buf, uint32_t reg_index, uint32_t elem)
		: buf(buf), reg_index(reg_index), elem(elem)
	{
	}
	
	reg_name(reg_name const& rhs)
		: buf(rhs.buf), reg_index(rhs.reg_index), elem(rhs.elem)
	{
	}
	
	reg_name& operator = (reg_name const& rhs)
	{
		buf = rhs.buf;
		reg_index = rhs.reg_index;
		elem = rhs.elem;
		return *this;
	}
	
	reg_name advance(size_t distance) const
	{
		return reg_name(buf, reg_index+distance, 0);
	}
	
	bool valid() const
	{
		return buf.slot == reg_slots::unknown;
	}
};

struct buffer_regs
{
public:
	buffer_regs(reg_slots slot, uint32_t index)
		: uid_(slot, index), total_reg_count(0)
	{
	}
	
	alloc_result alloc_reg(size_t sz, reg_name const& rname)
	{	
		reg_name reg_end;
		return alloc_reg(reg_end, rname, sz);
	}
	
	alloc_result alloc_reg(size_t sz, reg_name const& rname)
	{
		reg_name reg_end;
		auto rslt = alloc_reg(reg_end, rname, sz);
		if( rslt != alloc_result::ok ) { return rslt; }
		
		assign_semantic(rname, reg_end, sv);
	}
	
	alloc_result alloc_reg(size_t sz, std::string const& vname, reg_name const& rname)
	{
		auto const& reg_beg = rname;
		auto reg_end = rname.advance( round_up(sz, 16) / 16 );
		auto reg_range = interval<reg_name>::right_open(reg_beg, reg_end);
		
		return assign_variable(reg_range, vname) ? alloc_result::ok : alloc_result::register_has_been_allocated;
	}
	
	alloc_result alloc_reg(size_t sz, std::string const& vname)
	{
		pending_vars_[slot].push_back( std::make_tuple(vname, buf, sz) );
	}
	
	void update_unspecified_reg_variable()
	{
		for(auto const& var_sz: pending_vars_)
		{
			reg_name beg, end;
			alloc_reg(beg, end, var_sz.second);
			assign_variable(beg, end, var_sz.first);
		}
	}
	
	void assign_semantic(reg_name const& beg, reg_name const& end, semantic_value const& sv_beg)
	{
		reg_sv_[beg] = sv_beg;
		
		for( uint32_t rindex = beg.reg_index+1; rindex <= end.reg_index; ++rindex)
		{
			auto rname = reg_name(beg.slot, beg.buffer_index, rindex, 0);
			reg_sv_[reg_name] = sv.advance_index(rindex - sv_beg.reg_index);
		}
	}
	
	reg_name find_reg(semantic_value const& sv) const
	{
		auto iter = sv_reg_.find(sv);
		if(iter == sv_reg_.end())
		{
			return reg_name::invalid();
		}
		return iter->second;
	}
	
	boost::icl::interval<reg_name>
			 find_reg(std::string    const& vname) const
	{
		auto iter = var_reg_.find(vname);
		if( iter == var_reg_.end() )
		{
			return boost::icl_interval<reg_name>();
		}
		return iter->second;
	}
	
	void update_addr()
	{
		total_reg_count_ = 0;
		for(auto& range_addr: used_regs_)
		{
			auto& reg_range = range_addr.first;
			uint32_t reg_count = reg_range.up.reg_index - reg_range.low.reg_index;
			range_addr.second = total_reg_count_;
			total_reg_count_ += reg_count;
		}
	}
	
	uint32_t used_reg_count() const
	{
		return total_reg_count_;
	}
	
private:
	alloc_result alloc_reg(reg_name& beg, reg_name& end, size_t sz)
	{
		auto& slot_used_regs = used_regs_;
		beg = slot_used_regs.empty() ? reg_name(uid_, 0, 0) : boost::icl::last(slot_used_regs).first.next();
		return alloc_reg(end, beg, sz);
	}
	
	alloc_result alloc_reg(reg_name& end, reg_name const& beg, size_t sz)
	{
		end = reg_name.advance( align_up(sz, 16) / 16 );
		used_regs_.add( make_pair(icl::right_open(beg, end), 0) );
		return alloc_result::ok;
	}
	
	bool assign_variable(interval<reg_name> const& regs, std::string const& reg_name)
	{
		if( used_regs_.intersects(reg_range) )
		{
			return false;
		}
		
		var_reg_[vname] = reg_range;
		return true;
	}
	
	buffer_uid	uid_;
	uint32_t	total_reg_count_;
	
	boost::icl::interval_map<reg_name, uint32_t /*physical_reg*/>
														used_regs_;
	std::vector< std::pair<std::string /*var name*/, uint32_t /*size*/> >
														pending_vars_;
	std::map<semantic_value, reg_name>					sv_reg_;
	std::map<reg_name, semantic_value>					reg_sv_;
	std::map<std::string, interval<reg_name>>			var_reg_;
	std::map<uint32_t /*reg index*/, uint32_t /*addr*/>	reg_addr_;
};

class rfile_sasl_mapping
{
public:
	void initialize(shader_profile prof);
	
	buffer_regs* buffer(reg_slots slot, uint32_t buf_index)
	{
		auto& slot_bufs = buffer_regs_[static_cast<uint32_t>(slot)];
		
		if( buf_index < slot_bufs.size() )
		{
			if(!slot_bufs[buf_index])
			{
				slot_bufs[buf_index].reset(new buffer_regs(slot, buf_index));
			}
			return slot_bufs[buf_index].get();
		}
		else 
		{
			return nullptr;
		}
	}
	
	buffer_regs* buffer(buffer_uid buf_uid)
	{
		return buffer(buf_uid.slot, buf_uid.index);
	}
	
	buffer_regs const* buffer(reg_slots slot, uint32_t buf_index) const
	{
		auto& slot_bufs = buffer_regs_[static_cast<uint32_t>(slot)];
		
		if( buf_index < slot_bufs.size() )
		{
			return slot_bufs[buf_index].get();
		}
		else 
		{
			return nullptr;
		}
	}
	
	buffer_regs const* buffer(buffer_uid buf_uid) const
	{
		return buffer(buf_uid.slot, buf_uid.index);
	}
	
	void update_reg_address()
	{
		for(uint32_t slot = static_cast<uint32_t>(reg_slots::unknown); slot = static_cast<uint32_t>(reg_slots::count); ++slot)
		{
			auto& slot_bufs		= buffer_regs_[slot];
			auto& slot_buf_addr = buf_start_addr_[slot];
			
			uint32_t total = 0;
			for(auto& buf: slot_bufs)
			{
				slot_buf_addr.push_back(total);
				total += buf.used_reg_count();
			}
			used_reg_count_[slot] = total;
		}
	}
	
	uint32_t used_reg_count(reg_slot slot)
	{
		auto& slot_bufs = buffer_regs_[static_cast<uint32_t>(slot)];
		uint32_t total = 0;
		for(auto& buf: slot_bufs)
		{
			total += buf.used_regsiter_count()
		}
	}
	
	uint32_t reg_addr(reg_name const& rname)
	{
		uint32_t slot = static_cast<uint32_t>(rname.buf.slot);
		return	buf_start_addr_[slot][rname.buf.index] +
				buffer_regs_[slot][rname.buf.index]->reg_addr(rname);
	}
	
private:
	std::vector< boost::unique_ptr<buffer_regs> >
				buffer_regs_	[static_cast<uint32_t>(reg_slots::count)];
	uint32_t	used_reg_count_	[static_cast<uint32_t>(reg_slots::count)];
	std::vector<uint32_t>
				buf_start_addr_	[static_cast<uint32_t>(reg_slots::count)];
				
};

class rfile_hlsl_bc_mapping
{
	
};
