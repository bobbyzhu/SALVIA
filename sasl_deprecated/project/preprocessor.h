#ifndef SASL_PREPROCESSOR_H
#define SASL_PREPROCESSOR_H

/* �ļ�Ԥ���� */
class preprocessor
{
	/**
	ʹ�ñ������Դ�ļ�����Ϊ���룬�����һ��Ԥ��������ʱ�ļ���
	Ԥ�������ļ���#lineָ���ⲻ��Я������Ԥ����ָ�
	�ú�������Ϊ�̰߳�ȫ�ĺ��������Զ��߳�ִ�С�
	**/
	virtual std::string preprocess(const std::string& filename) = 0;
};

#endif // preprocessor_H__