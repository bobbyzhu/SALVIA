#ifndef SASL_TRIE_MAP_H_29F5A412_2091_45AE_AEBB_30771ECAB49E
#define SASL_TRIE_MAP_H_29F5A412_2091_45AE_AEBB_30771ECAB49E

#include <boost/smart_ptr.hpp>
#include <boost/tr1/tr1/unordered_map>
#include <vector>
#include <algorithm>

template< typename KeyT, typename ValueT >
class trie_map{
public:
	typedef typename child_maps_t::iterator child_iterator_t;
	typedef typename child_maps_t::const_iterator const_child_iterator_t;

	//��ά�ȵĲ�����ʼ��
	trie_map(){
	}

	//�Ƿ����Key�����keylst���ȴ���ά�ȣ�����û�в鵽��Ӧ��key���򷵻�false��
	template< typename KeyIterT >
	bool has_key( KeyIterT keylst_begin, KeyIterT keylst_end ) const{
		const child_maps_t* sub_map;
		KeyIterT macthed_pos;
		search( keylst_begin, keylst_end, sub_map, matched_pos );

		if( matched_pos == keylst_end && sub_map != NULL ){
			return true;
		}

		return false;
	}

	//���ĳһά�ȵ�Keys�����keylst���ȴ���ά�ȣ�����δ��������Ӧά�ȣ��򷵻ؿ��б�
	template< typename KeyIterT >
	const std::vector< KeyT >& keys( KeyIterT keylst_begin, KeyIterT keylst_end ) const{
		const child_maps_t* sub_map;
		KeyIterT macthed_pos;
		search( keylst_begin, keylst_end, sub_map, matched_pos );

		if( matched_pos == keylst_end && sub_map != NULL ){
			if( sub_map.is_leaf() ){
				return vector< KeyT >();
			}
			return sub_map->child_maps_.keys();
		}
	}

	//����һ��ֵ��keylst������Ҫ��ά����ͬ������ᵼ��ʧ�ܡ�
	template< typename KeyIterT >
	bool insert( KeyIterT keylst_begin, KeyIterT keylst_end, const ValueT& val, bool can_replace = false ){
		this_type* sub_map;
		KeyIterT matched_pos;
		search( keylst_begin, keylst_end, sub_map, matched_pos );

		// �����ƥ���꣬�ҵ�ǰ�ڵ㲻���滻������false
		if( matched_pos == keylst_end && ! can_replace ){
			return false;
		}

		//������·����ע�⣬�����ȫƥ�䣬��ôsub_map��Ȼ������Ҷ�ڵ��ϡ�
		for( KeyIterT cur_key_pos = matched_pos; cur_key_pos != keylst_end; ++cur_key_pos ){
			boost::shared_ptr<this_type> next_map( new this_type(sub_map->ndim_ - 1) );
			(sub_map->child_maps_)[*cur_key_pos] = next_map;
			sub_map = next_map.get();
		}
		sub_map->value_ = val;

		return true;
	}

	//��ѯһ��ֵ�����û���ҵ���Ӧ��ֵ���򷵻��쳣��
	template< typename KeyIterT >
	const ValueT& lookup( KeyIterT keylst_begin, KeyIterT keylst_end ) const{
		const this_type* sub_map;
		KeyIterT matched_pos;
		search( keylst_begin, keylst_end, sub_map, matched_pos );

		if( matched_pos == keylst_end ){
			return sub_map->value_;
		}

		throw key_not_found_exception();
	}

	//��ѯֵ��
	template< typename KeyIterT >
	bool lookup( KeyIterT keylst_begin, KeyIterT keylst_end, ValueT& ret ) const{
		const this_type* sub_map;
		KeyIterT matched_pos;
		search( keylst_begin, keylst_end, sub_map, matched_pos );

		if( matched_pos == keylst_end ){
			ret = sub_map->value_;
			return true;
		}
		return false;
	}

	child_iterator_t child_begin(){
		return child_maps_.begin();
	}
	
	child_iterator_t child_end(){
		return child_maps_.end();
	}
	
	const_child_iterator_t child_begin() const{
		return child_maps_.begin();
	}
	
	const_child_iterator_t child_end() const{
		return child_maps_.end();
	}
	
	bool is_first_child();
	bool is_last_child();
	
private:
	typedef trie_map< KeyT, ValueT > this_type;
	typedef std::tr1::unordered_map< KeyT, boost::shared_ptr< this_type > > child_maps_t;
	
	trie_map( const this_type* parent );
	
	/****************************************
	������������Ӧ�Ľڵ㡣
	����ƥ���Key����ƥ�����������Ľڵ㡣
	�磺key ���� (1, 7, 8, end)
	�Լ���root������ƥ�����
					  root
					 /    \
					/      \
return node ->	   1        3
				  / \      / \
				 /   \    /   \
				2     6  4     5
			   / \   / \      / \
			  /   \ /   \    /   \
		  ...  ...  ...  ...  ...  ...

	return remaining key sequence: ( 7, 8, end )

	****************************************************/
	template< typename KeyIterT >
	void search( KeyIterT begin, KeyIterT end, const this_type*& sub_map, KeyIterT& matched_key_pos ) const{
		const this_type* cur_map = this;
		KeyIterT cur_key_iter;

		for( cur_key_iter = begin; cur_key_iter != end; ++cur_key_iter ){
			const this_type* next_map = cur_map->find_child( *cur_key_iter );
			if ( next_map ){
				cur_map = next_map;
			} else {
				break;
			}
		}

		matched_key_pos = cur_key_iter;
		sub_map = cur_map;
	}

	template< typename KeyIterT >
	void search( KeyIterT begin, KeyIterT end, this_type*& sub_map, KeyIterT& matched_key_pos ){
		this_type* cur_map = this;
		KeyIterT cur_key_iter;

		for( cur_key_iter = begin; cur_key_iter != end; ++cur_key_iter ){
			this_type* next_map = cur_map->find_child( *cur_key_iter );
			if ( next_map ){
				cur_map = next_map;
			} else {
				break;
			}
		}

		matched_key_pos = cur_key_iter;
		sub_map = cur_map;
	}

	this_type* find_child( const KeyT& key ){
		child_maps_t::iterator child_iter = child_maps_.find( key );
		if ( child_iter == child_maps_.end() ){
			return NULL;
		}
		return (child_iter->second).get();
	}

	const this_type* find_child( const KeyT& key ) const{
		child_maps_t::const_iterator child_iter = child_maps_.find( key );
		if ( child_iter == child_maps_.end() ){
			return NULL;
		}
		return (child_iter->second).get();
	}

	bool is_leaf() const{
		return child_maps_.empty();
	}

	child_maps_t child_maps_;
	ValueT value_;
	const this_type* parent_;
};

#endif //SASL_COMPILER_trie_map_H_29F5A412_2091_45AE_AEBB_30771ECAB49E