float test_no_overloaded_function()
{
	return cross(5);
}

struct not_a_member
{
	int f;
};

int test_not_a_member()
{
	not_a_member n;
	return n.g;
}

int3 test_invalid_swizzle()
{
	int2 x = int2(0, 5);
	return x.xyw;
}

int test_member_left_must_have_struct()
{
	sampler s;
	return s.k;
}

int cond_expr_cannot_convert_from()
{
	sampler s;
	return s ? 1 : s;
}

int test_operator_unmathced_type()
{
	sampler s;
	float r;
	return 1 + s;
}