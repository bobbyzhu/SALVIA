// ���� ifdef ���Ǵ���ʹ�� DLL �������򵥵�
// ��ı�׼�������� DLL �е������ļ��������������϶���� SASL_SYNTAX_TREE_GENERATOR_EXPORTS
// ���ű���ġ���ʹ�ô� DLL ��
// �κ�������Ŀ�ϲ�Ӧ����˷��š�������Դ�ļ��а������ļ����κ�������Ŀ���Ὣ
// SASL_SYNTAX_TREE_GENERATOR_API ������Ϊ�Ǵ� DLL ����ģ����� DLL ���ô˺궨���
// ������Ϊ�Ǳ������ġ�
#ifdef SASL_SYNTAX_TREE_GENERATOR_EXPORTS
#define SASL_SYNTAX_TREE_GENERATOR_API __declspec(dllexport)
#else
#define SASL_SYNTAX_TREE_GENERATOR_API __declspec(dllimport)
#endif

// �����Ǵ� sasl_syntax_tree_generator.dll ������
class SASL_SYNTAX_TREE_GENERATOR_API Csasl_syntax_tree_generator {
public:
	Csasl_syntax_tree_generator(void);
	// TODO: �ڴ�������ķ�����
};

extern SASL_SYNTAX_TREE_GENERATOR_API int nsasl_syntax_tree_generator;

SASL_SYNTAX_TREE_GENERATOR_API int fnsasl_syntax_tree_generator(void);
