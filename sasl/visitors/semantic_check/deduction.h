/***********************************************************************************
	�����Ƶ������Ƶ����۷�Ϊ�ļ���
		ͬһ�����ݡ���ת���Ͳ����Ƶ���
		ͬһ�Ƶ�ָԴ���ͺ�Ŀ������ֵ����ͬ���Ƶ����ȼ���is_no_deduction��
		�����Ƶ�ָĿ�����͵�ֵ����Դ���͵ĳ������Ƶ����ȼ���is_successful - is_no_deduction��
		��ת���Ƶ�ָԴ������Ŀ�����͵�ֵ����ڽ������Ƶ����ȼ���is_maybe_unexcepted - is_successful��
		�����Ƶ�ָԴ���ͺ�Ŀ������ֵ���ཻ�����岻һ�µ��Ƶ����ȼ���is_failed��
		��Deducers����У�û����ʽ��ע���Ƶ�����Ϊ�ǲ����Ƶ��ġ�

	����-�����Ƶ�����
		�ڻ������͵ĳ��������Ƶ������У����ڻ������͵�ֵ���ǿ���Ԥ���ж��ģ�
		��˽����������Ƶ��Ͳ����Ƶ���������������Ƶ�ʱ�������ھ��档�����Ƶ�ʱ����Ҫָ������

	����-�����Ƶ�����
		�ӳ������͸�ֵ���������᳢�Խ��������Ա任�ĳ����������ͱ任��ֵ��������
		����������Ա任������Դ���򾯸����ʽ������ʽ�任������
		�����ʽ�任�����������ͬ�ڳ����任�Ĳ����Ƶ����д���

	����-�����Ƶ�����
		����-�������Ƶ�������ļ���ʽ���Ƶ����ۡ�
		fail��is_maybe_unexcepted��������ʽ�Ĳ���error��warning�ı�������Ϣ��
		���ת�����Ϊis_no_deduction��is_successful������Ϊת���ǿ���ʽ�ġ�
		��ת�����Ϊis_maybe_unexcepted����Ҫ�ж�ת������ʽת��������ʽת����
		�������ʽת������ῼ��can_implicit�������ʽת�����ܾ������Ƶ����Զ�������Ӧ���档
		is_failed��ʼ�շ���ƥ�����
************************************************************************************/
class deduction{
public:
	static deduction from_scalar_type( buildin_types sasl_type );
	static deduction from_vec_type( buildin_types scalar_type, size_t len );
	static deduction from_mat_type( buildin_types scalar_type, size_t rowcnt, size_t colcnt );

	h_ast_node_type type;
	boost::any value;
	symbol var;

	// if type is same, there is no deduction.
	bool is_no_deduction(){
		return cost_ < match_cost::lossless();
	}
	
	// if deduction executed w/out warning, it is called sucessful
	bool is_sucessful(){
		return cost_ < match_cost::loss();
	}
	
	// if decution maybe leads to unexcepted behaviour, it returns "true"  
	bool is_maybe_unexcepted(){
		return ! is_failed();
	}
	
	// type conversation or value evaluation is failed.
	bool is_failed(){
		return is_err_;
	}
	
	deduction& is_no_deduction( bool v ){
		match_cost()
	}
	
	deduction& is_successful( bool v);
	deduction& is_maybe_unexcepted( bool v );
	deduction& is_failed( bool v );
	deduction& can_implicit( bool v );

	match_cost cost(){
		return cost_;
	}
	
	deduction& cost( match_cost v ){
		cost_ = v;
	}
	
	deduction& add_cost( match_cost v ){
		cost_ += v;
	}

	h_compiler_warning warning(){
		return warning_;
	}
	
	h_compiler_error error(){
		return error_;
	}
	
	deduction& warning( h_compiler_warning v ){
		warning_ = v;
		return *this;
	}
	
	deduction& error( h_compiler_error v ){
		error_ = v;
		return *this;
	}

	//Ԥ�����ת��״̬���á�
	deduction& small_to_large( size_t cost )		// e.g. int8 -> int16
	{
		cost_ = match_cost( 0, cost, 0 );
	}
	
	deduction& large_to_small( size_t cost )		// e.g. int16 -> int8
	{
		cost_ = match_cost( cost, 0, 0 );
	}
	
	deduction& change_sign( size_t cost )		// e.g. int8 <--> uint8, int8 <--> uint16
	{
		cost_ = match_cost( cost, 0, 0 );
	}

private:
	h_compiler_warning warning_;
	h_compiler_error error_;

	bool can_implicit_;
	bool no_conv_;
	bool is_err_;
	
	match_cost cost_;
};