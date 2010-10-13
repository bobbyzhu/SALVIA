/********************************************************************
Copyright (C) 2007-2008 Ye Wu

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

created:	2008/05/30
author:		Ye Wu

purpose:
slog��һ������������־ϵͳ��

Modify Log:
2008/06/06 - �����Log�ĵ�����д�� By Ye Wu
*********************************************************************/
#ifndef slog_h__
#define slog_h__

#include "log_serializer.h"

#include "util.h"
#include "detail/typedefs.h"
#include "detail/stringx.h"

#ifdef EFLIB_MSVC
#pragma warning(push)
#pragma warning(disable : 6011)
#endif
#include <boost/smart_ptr.hpp>
#ifdef EFLIB_MSVC
#pragma warning(pop)
#endif
#include <boost/function.hpp>

#include <vector>

namespace efl{
	//////////////////////////////////////////////////////////////////////////
	//��־�ȼ�����.
	//ÿ��һλ����һ�Σ��м��λ�ο��������û��Զ����á�
	//���һλ������DEBUG/RELEASEģʽ��

	const uint32_t LOGLEVEL_FATAL_ERROR = 0x8000;
	const uint32_t LOGLEVEL_ERROR = 0x2000;
	const uint32_t LOGLEVEL_WARNING = 0x0800;
	const uint32_t LOGLEVEL_MESSAGE = 0x0200;
	const uint32_t LOGLEVEL_FOOTPRINT = 0x0080;
	const uint32_t LOGLEVEL_DEBUGONLY = 0x0001;

	//////////////////////////////////////////////////////////////////////////
	//�򿪷�ʽ��׷�ӻ���ɾ����
	const uint32_t LOG_OPENMODE_APPEND = 0x0;
	const uint32_t LOG_OPENMODE_WRITE = 0x1;

	//////////////////////////////////////////////////////////////////////////
	//�Ƚ�����������֤log�Ƿ�ᱻ��¼
	struct log_level_cmp_equal{
		bool operator()(const uint32_t cur_log_level, const uint32_t ref_log_level){
			return cur_log_level == ref_log_level;
		}
	};

	struct log_level_cmp_greater
	{
		bool operator()(const uint32_t cur_log_level, const uint32_t ref_log_level){
			return cur_log_level > ref_log_level;
		}
	};

	struct log_level_cmp_ge
	{
		bool operator()(const uint32_t cur_log_level, const uint32_t ref_log_level){
			return cur_log_level >= ref_log_level;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	//slog��״̬�������ڱ�����ָ�״̬��
	template<class T> class slog;

	template<class SerializerType>
	class slog_state
	{
	public:
		typedef slog<SerializerType> slog_type;

		void load_state(const slog_type* pslog)
		{
			ref_log_level_ = pslog->ref_log_level_;
			loglvlcmp_ = pslog->loglvlcmp_;
			h_logserializer = pslog->h_logserializer;
		}

		void set_state(slog_type* pslog) const
		{
			pslog->ref_log_level_ = ref_log_level_;
			pslog->loglvlcmp_ = loglvlcmp_;
			pslog->h_logserializer = h_logserializer;
		}

	private:
		uint32_t ref_log_level_;
		boost::function<bool (uint32_t, uint32_t)> loglvlcmp_;
		boost::shared_ptr<SerializerType> h_logserializer;
	};

	//slog״̬ջ����
	template<class SerializerType>
	class slog_state_stack
	{
	public:
		typedef slog_state<SerializerType> state_type;

		boost::shared_ptr<const state_type> pop()
		{
			boost::shared_ptr<const state_type > ret = states_.back();
			states_.pop_back();
			return ret;
		}

		void push(boost::shared_ptr<const state_type> hstate)
		{
			states_.push_back(hstate);
		}

		void push(const state_type* pstate)
		{
			states_.push_back(boost::shared_ptr<const state_type>(pstate));
		}

	private:
		std::vector<boost::shared_ptr<const state_type> > states_;
	};

	// slog�ࡣslog����soft art log�ļ�ƣ�Ҳ��simple log�ļ�� :-)
	// slog���ⲿ������ʵ��࣬���л�����log_serializer�У�������Ҫ������ʿ��ơ�
	template <class SerializerType>
	class slog
	{
		template<class T> friend class slog_state;
	public:
		typedef SerializerType serializer_type;
		typedef slog<SerializerType> this_type;
		typedef slog_state<SerializerType> state_type;

		static boost::shared_ptr<this_type> create(const uint32_t ref_log_level, uint32_t log_open_mode)
		{
			UNREF_PARAM(log_open_mode);
			return boost::shared_ptr<this_type>(new this_type(ref_log_level));
		}

		//����һ��log��Ĭ�ϵ�log�ȼ�Ϊ�������е�log
		slog(const uint32_t ref_log_level = 0) :loglvlcmp_(log_level_cmp_ge()), ref_log_level_(ref_log_level)
		{
		}

		//״̬���ú���
		void set_serializer(boost::shared_ptr<serializer_type> hsrl)
		{
			h_logserializer = hsrl;
		}

		void set_log_level(uint32_t loglvl)
		{
			ref_log_level_ = loglvl;
		}

		void set_comparer(boost::function<bool (uint32_t, uint32_t)> cmpr)
		{
			loglvlcmp_ = cmpr;
		}

		//���溯��log state�ĵ�ǰ״̬
		void push_current_log_state()
		{
			state_type* pstate = new state_type();
			pstate->load_state(this);
			state_stack_.push(pstate);
		}

		//�ָ�log state����һ��״̬��
		void pop_current_log_state()
		{
			boost::shared_ptr<const state_type > hstate = state_stack_.pop();
			hstate->set_state(this);
		}

		void begin_log()
		{
			if(!h_logserializer) return;

			h_logserializer->begin_log();
		}

		void end_log()
		{
			if(!h_logserializer) return;

			h_logserializer->end_log();
		}

		template <class T>
		bool write(const std::_tstring& key, const T& val, const uint32_t cur_log_level)
		{
			if(!loglvlcmp_(cur_log_level, ref_log_level_))
			{
				return false;
			}
			if (!h_logserializer)
			{
				return false;
			}

			h_logserializer->write(key, val);
			return true;
		}

		template<class T>
		bool write(const T& val, const uint32_t cur_log_level)
		{
			if(!loglvlcmp_(cur_log_level, ref_log_level_))
			{
				return false;
			}
			if (!h_logserializer)
			{
				return false;
			}

			h_logserializer->write(val);
			return true;
		}

	private:
		uint32_t ref_log_level_;
		boost::function<bool (uint32_t, uint32_t)> loglvlcmp_;
		boost::shared_ptr<SerializerType> h_logserializer;

		slog_state_stack<SerializerType> state_stack_;
	};

	//////////////////////////////////////////////////////////////////////////
	//����״̬����ջ����ԡ�

	//slog״̬�Ľ���ջ
	template <class T>
	class log_state_scope
	{
	public:
		log_state_scope(T* psloger):psloger_(psloger){
			psloger->push_current_log_state();
		}

		~log_state_scope()
		{
			psloger_->pop_current_log_state();
		}
	private:
		T* psloger_;
	};

	//���л�����״̬ջ��
	template<class T>
	class log_serializer_state_scope
	{
	public:
		log_serializer_state_scope(T* psrl):psrl_(psrl)
		{
			psrl->push_token_state();
		}
		~log_serializer_state_scope()
		{
			psrl_->pop_token_state();
		}
	private:
		T* psrl_;
	};

	//���л�ʱ��ι�ϵ�򡣹���ʱ����һ���Ӳ㣬����ʱ�˳��������ϡ�
	template<class T>
	class log_serializer_indent_scope
	{
	public:
		log_serializer_indent_scope(T* pserializer):pserializer_(pserializer){
			pserializer->begin_log();
		}

		~log_serializer_indent_scope()
		{
			pserializer_->end_log();
		}

	private:
		T* pserializer_;
	};

	//ʵ�ʵ�logϵͳ������ģʽ
	template<class SlogType>
	class log_system
	{
	public:
		typedef SlogType slog_type;
		typedef boost::shared_ptr<typename slog_type::serializer_type> h_serializer;

		static SlogType& instance(boost::shared_ptr<std::_tostream> hos = boost::shared_ptr<std::_tostream>())
		{
			static bool is_first_run = true;
			static SlogType slog;
			if (is_first_run)
			{
				h_serializer hsrl;
				if (hos){
					hsrl = h_serializer(new (typename slog_type::serializer_type)(*hos, _EFLIB_T("\t"), _EFLIB_T("\n"), _EFLIB_T("=")));
				}
				slog.set_serializer(hsrl);
				is_first_run = false;
			}
			return slog;
		}
	};
}



#endif // slog_h__
