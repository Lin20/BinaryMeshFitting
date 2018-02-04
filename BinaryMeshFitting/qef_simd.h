#ifndef		HAS_QEF_SIMD_H_BEEN_INCLUDED
#define		HAS_QEF_SIMD_H_BEEN_INCLUDED

//
// Quadric Error Function / Singluar Value Decomposition SSE2 implementation
// Public domain
//
// Input is a set of positions / vertices of a surface and the surface normals
// at these positions (i.e. Hermite data). A "best fit" position is calculated
// from these positions along with an error value.
//
// Exmaple use cases:
//   - placing a vertex inside a voxel/node for algorithms like Dual Contouring
//     (the positions and normals corresponding to edge/isosurface intersections)
//   - placing the vertex resulting from collapsing an edge when simplifying a mesh
//     (the positions and normals of the collapsed edge should be supplied)
//
// Usage:
// 
// SSE registers:
//
//	__m128 position[2] = { ... };
//	__m128 normal[2] = { ... };
//	__m128 solvedPos;
//	float error = qef_solve_from_points(position, normal, 2, &solvedPos);
//
// 4D vectors:
//
//	__declspec(align(16)) float positions[2 * 4] = { ... };
//	__declspec(align(16)) float normals[2 * 4] = { ... };
//	__declspec(align(16)) float solved[4];
//	float error = qef_solve_from_points_4d(positions, normals, 2, solvedPos);
//
// 3D vectors (or struct with 3 float members, e.g. glm::vec3):
//
//	glm::vec3 positions[2] = { ... };
//	glm::vec3 normals[2] = { ... };
//	glm::vec3 solvedPos;
//	float error = qef_solve_from_points_3d(&positions[0].x, &normals[0].x, 2, &solvedPos.x);
//

#include	<xmmintrin.h>
#include	<immintrin.h>

const int QEF_MAX_INPUT_COUNT = 12;

// Ideally the data would already be in SSE registers & returned in a SEE register
float qef_solve_from_points(
	const __m128* positions,
	const __m128* normals,
	const int count,
	__m128* solved_position);

// Expects 4d vectors contiguous in memory for the positions/normals
// Addresses pointed to by positions, normals and solved_position MUST be 16 byte aligned
// Writes result to 4d vector.
float qef_solve_from_points_4d(
	const float* positions,
	const float* normals,
	const int count,
	float* solved_position);

float qef_solve_from_points_4d_interleaved(
	const float* data,
	const size_t stride,
	const int count,
	float* solved_position);

// Expects 3d vectors contiguous in memory for the positions/normals
// No alignment requirements.
// Writes result to 3d vector.
float qef_solve_from_points_3d(
	const float* positions,
	const float* normals,
	const int count,
	float* solved_position);


#ifdef QEF_INCLUDE_IMPL

union Mat4x4
{
	float	m[4][4];
	__m128	row[4];
};

#define SVD_NUM_SWEEPS 5
const float PSUEDO_INVERSE_THRESHOLD = 0.001f;

// ----------------------------------------------------------------------------

static inline __m128 vec4_abs(const __m128& x)
{
	static const __m128 mask = _mm_set1_ps(-0.f);
	return _mm_andnot_ps(mask, x);
}

// ----------------------------------------------------------------------------

static inline float vec4_dot(const __m128& a, const __m128& b)
{
	// apparently _mm_dp_ps isn't well implemented in hardware so this "basic" version is faster
	__m128 mul = _mm_mul_ps(a, b);
	__m128 s0 = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
	__m128 add = _mm_add_ps(mul, s0);
	__m128 s1 = _mm_shuffle_ps(add, add, _MM_SHUFFLE(0, 1, 2, 3));
	__m128 res = _mm_add_ps(add, s1);
	return _mm_cvtss_f32(res);
}

// ----------------------------------------------------------------------------

static inline __m128 vec4_mul_m4x4(const __m128& a, const Mat4x4& B)
{
	auto bob = _mm_shuffle_ps(a, a, 0x00);

	__m128 result = _mm_mul_ps(_mm_shuffle_ps(a, a, 0x00), B.row[0]);
	result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0x55), B.row[1]));
	result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xaa), B.row[2]));
	result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xff), B.row[3]));
	return result;
}

// ----------------------------------------------------------------------------

static inline __m256 avx_vec4_mul_m4x4(const __m256& a, const Mat4x4& B)
{
	__m256 result;
	result = _mm256_mul_ps(_mm256_shuffle_ps(a, a, 0x00), _mm256_broadcast_ps(&B.row[0]));
	result = _mm256_add_ps(result, _mm256_mul_ps(_mm256_shuffle_ps(a, a, 0x55), _mm256_broadcast_ps(&B.row[1])));
	result = _mm256_add_ps(result, _mm256_mul_ps(_mm256_shuffle_ps(a, a, 0xaa), _mm256_broadcast_ps(&B.row[2])));
	result = _mm256_add_ps(result, _mm256_mul_ps(_mm256_shuffle_ps(a, a, 0xff), _mm256_broadcast_ps(&B.row[3])));

	return result;
}

// ----------------------------------------------------------------------------

static void m4x4_mul_m4x4(Mat4x4& out, const Mat4x4& A, const Mat4x4& B)
{
	out.row[0] = vec4_mul_m4x4(A.row[0], B);
	out.row[1] = vec4_mul_m4x4(A.row[1], B);
	out.row[2] = vec4_mul_m4x4(A.row[2], B);
	out.row[3] = vec4_mul_m4x4(A.row[3], B);
}


// ----------------------------------------------------------------------------

static void givens_coeffs_sym(__m128& c_result, __m128& s_result, const Mat4x4& vtav, const int a, const int b)
{
	__m128 simd_pp = _mm_set_ps(
		0.f,
		vtav.row[a].m128_f32[a],
		vtav.row[a].m128_f32[a],
		vtav.row[a].m128_f32[a]);

	__m128 simd_pq = _mm_set_ps(
		0.f,
		vtav.row[a].m128_f32[b],
		vtav.row[a].m128_f32[b],
		vtav.row[a].m128_f32[b]);

	__m128 simd_qq = _mm_set_ps(
		0.f,
		vtav.row[b].m128_f32[b],
		vtav.row[b].m128_f32[b],
		vtav.row[b].m128_f32[b]);

	static const __m128 zeros = _mm_set1_ps(0.f);
	static const __m128 ones = _mm_set1_ps(1.f);
	static const __m128 twos = _mm_set1_ps(2.f);

	// tau = (a_qq - a_pp) / (2.f * a_pq);
	__m128 pq2 = _mm_mul_ps(simd_pq, twos);
	__m128 qq_sub_pp = _mm_sub_ps(simd_qq, simd_pp);
	__m128 tau = _mm_div_ps(qq_sub_pp, pq2);

	// stt = sqrt(1.f + tau * tau);
	__m128 tau_sq = _mm_mul_ps(tau, tau);
	__m128 tau_sq_1 = _mm_add_ps(tau_sq, ones);
	__m128 stt = _mm_sqrt_ps(tau_sq_1);

	// tan = 1.f / ((tau >= 0.f) ? (tau + stt) : (tau - stt));
	__m128 tan_gt = _mm_add_ps(tau, stt);
	__m128 tan_lt = _mm_sub_ps(tau, stt);
	__m128 tan_cmp = _mm_cmpge_ps(tau, zeros);
	__m128 tan_cmp_gt = _mm_and_ps(tan_cmp, tan_gt);
	__m128 tan_cmp_lt = _mm_andnot_ps(tan_cmp, tan_lt);
	__m128 tan_inv = _mm_or_ps(tan_cmp_gt, tan_cmp_lt);
	__m128 tan = _mm_div_ps(ones, tan_inv);

	// c = rsqrt(1.f + tan * tan);
	__m128 tan_sq = _mm_mul_ps(tan, tan);
	__m128 tan_sq_1 = _mm_add_ps(ones, tan_sq);
	__m128 c = _mm_rsqrt_ps(tan_sq_1);

	// s = tan * c;
	__m128 s = _mm_mul_ps(tan, c);

	// if pq == 0.0: c = 1.f, s = 0.f
	__m128 pq_cmp = _mm_cmpeq_ps(simd_pq, zeros);

	__m128 c_true = _mm_and_ps(pq_cmp, ones);
	__m128 c_false = _mm_andnot_ps(pq_cmp, c);
	c_result = _mm_or_ps(c_true, c_false);

	__m128 s_true = _mm_and_ps(pq_cmp, zeros);
	__m128 s_false = _mm_andnot_ps(pq_cmp, s);
	s_result = _mm_or_ps(s_true, s_false);
}

// ----------------------------------------------------------------------------

static void rotateq_xy(Mat4x4& vtav, const __m128& c, const __m128& s, const int a, const int b)
{
	__m128 u = _mm_set_ps(
		0.f,
		vtav.row[a].m128_f32[a],
		vtav.row[a].m128_f32[a],
		vtav.row[a].m128_f32[a]);

	__m128 v = _mm_set_ps(
		0.f,
		vtav.row[b].m128_f32[b],
		vtav.row[b].m128_f32[b],
		vtav.row[b].m128_f32[b]);

	__m128 A = _mm_set_ps(
		0.f,
		vtav.row[a].m128_f32[b],
		vtav.row[a].m128_f32[b],
		vtav.row[a].m128_f32[b]);

	static const __m128 twos = _mm_set1_ps(2.f);

	__m128 cc = _mm_mul_ps(c, c);
	__m128 ss = _mm_mul_ps(s, s);

	// mx = 2.0 * c * s * A;
	__m128 c2 = _mm_mul_ps(twos, c);
	__m128 c2s = _mm_mul_ps(c2, s);
	__m128 mx = _mm_mul_ps(c2s, A);

	// x = cc * u - mx + ss * v;
	__m128 x0 = _mm_mul_ps(cc, u);
	__m128 x1 = _mm_sub_ps(x0, mx);
	__m128 x2 = _mm_mul_ps(ss, v);
	__m128 x = _mm_add_ps(x1, x2);

	// y = ss * u + mx + cc * v;
	__m128 y0 = _mm_mul_ps(ss, u);
	__m128 y1 = _mm_add_ps(y0, mx);
	__m128 y2 = _mm_mul_ps(cc, v);
	__m128 y = _mm_add_ps(y1, y2);


	vtav.row[a].m128_f32[a] = x.m128_f32[0];
	vtav.row[b].m128_f32[b] = y.m128_f32[0];
}

// ----------------------------------------------------------------------------

static void rotate_xy(Mat4x4& vtav, Mat4x4& v, float c, float s, const int& a, const int& b)
{
	__m128 simd_u = _mm_set_ps(
		vtav.row[0].m128_f32[3 - b],
		v.row[2].m128_f32[a],
		v.row[1].m128_f32[a],
		v.row[0].m128_f32[a]);

	__m128 simd_v = _mm_set_ps(
		vtav.row[1 - a].m128_f32[2],
		v.row[2].m128_f32[b],
		v.row[1].m128_f32[b],
		v.row[0].m128_f32[b]);

	__m128 simd_c = _mm_load1_ps(&c);
	__m128 simd_s = _mm_load1_ps(&s);

	__m128 x0 = _mm_mul_ps(simd_c, simd_u);
	__m128 x1 = _mm_mul_ps(simd_s, simd_v);
	__m128 x = _mm_sub_ps(x0, x1);

	__m128 y0 = _mm_mul_ps(simd_s, simd_u);
	__m128 y1 = _mm_mul_ps(simd_c, simd_v);
	__m128 y = _mm_add_ps(y0, y1);

	v.row[0].m128_f32[a] = x.m128_f32[0];
	v.row[1].m128_f32[a] = x.m128_f32[1];
	v.row[2].m128_f32[a] = x.m128_f32[2];
	vtav.row[0].m128_f32[3 - b] = x.m128_f32[3];

	v.row[0].m128_f32[b] = y.m128_f32[0];
	v.row[1].m128_f32[b] = y.m128_f32[1];
	v.row[2].m128_f32[b] = y.m128_f32[2];
	vtav.row[1 - a].m128_f32[2] = y.m128_f32[3];

	vtav.row[a].m128_f32[b] = 0.f;
}

// ----------------------------------------------------------------------------

static __m128 svd_solve_sym(Mat4x4& v, const Mat4x4& a)
{
	Mat4x4 vtav = a;

	for (int i = 0; i < SVD_NUM_SWEEPS; ++i)
	{
		__m128 c, s;

		if (vtav.row[0].m128_f32[1] != 0.f)
		{
			givens_coeffs_sym(c, s, vtav, 0, 1);
			rotateq_xy(vtav, c, s, 0, 1);
			rotate_xy(vtav, v, c.m128_f32[1], s.m128_f32[1], 0, 1);
			vtav.row[0].m128_f32[1] = 0.f;
		}

		if (vtav.row[0].m128_f32[2] != 0.f)
		{
			givens_coeffs_sym(c, s, vtav, 0, 2);
			rotateq_xy(vtav, c, s, 0, 2);
			rotate_xy(vtav, v, c.m128_f32[1], s.m128_f32[1], 0, 2);
			vtav.row[0].m128_f32[2] = 0.f;
		}

		if (vtav.row[1].m128_f32[2] != 0.f)
		{
			givens_coeffs_sym(c, s, vtav, 1, 2);
			rotateq_xy(vtav, c, s, 1, 2);
			rotate_xy(vtav, v, c.m128_f32[2], s.m128_f32[2], 1, 2);
			vtav.row[1].m128_f32[2] = 0.f;
		}
	}

	return _mm_set_ps(
		0.f,
		vtav.row[2].m128_f32[2],
		vtav.row[1].m128_f32[1],
		vtav.row[0].m128_f32[0]);
}

// ----------------------------------------------------------------------------

static inline __m128 svd_invdet(const __m128& x)
{
	static const __m128 ones = _mm_set1_ps(1.f);
	static const __m128 tol = _mm_set1_ps(PSUEDO_INVERSE_THRESHOLD);

	__m128 abs_x = vec4_abs(x);
	__m128 one_over_x = _mm_div_ps(ones, x);
	__m128 abs_one_over_x = vec4_abs(one_over_x);
	__m128 min_abs = _mm_min_ps(abs_x, abs_one_over_x);
	__m128 cmp = _mm_cmpge_ps(min_abs, tol);
	return _mm_and_ps(cmp, one_over_x);
}

// ----------------------------------------------------------------------------

static void svd_pseudoinverse(Mat4x4& o, const __m128& sigma, const Mat4x4& v)
{
	const __m128 invdet = svd_invdet(sigma);

	Mat4x4 m;
	m.row[0] = _mm_mul_ps(v.row[0], invdet);
	m.row[1] = _mm_mul_ps(v.row[1], invdet);
	m.row[2] = _mm_mul_ps(v.row[2], invdet);
	m.row[3] = _mm_set1_ps(0.f);

	o.row[0].m128_f32[0] = vec4_dot(m.row[0], v.row[0]);
	o.row[0].m128_f32[1] = vec4_dot(m.row[1], v.row[0]);
	o.row[0].m128_f32[2] = vec4_dot(m.row[2], v.row[0]);
	o.row[0].m128_f32[3] = 0.f;

	o.row[1].m128_f32[0] = vec4_dot(m.row[0], v.row[1]);
	o.row[1].m128_f32[1] = vec4_dot(m.row[1], v.row[1]);
	o.row[1].m128_f32[2] = vec4_dot(m.row[2], v.row[1]);
	o.row[1].m128_f32[3] = 0.f;

	o.row[2].m128_f32[0] = vec4_dot(m.row[0], v.row[2]);
	o.row[2].m128_f32[1] = vec4_dot(m.row[1], v.row[2]);
	o.row[2].m128_f32[2] = vec4_dot(m.row[2], v.row[2]);
	o.row[2].m128_f32[3] = 0.f;

	o.row[3] = m.row[3];
}

// ----------------------------------------------------------------------------


static void svd_solve_ATA_ATb(const Mat4x4& ATA, const __m128& ATb, __m128& x)
{
	Mat4x4 V;
	V.row[0] = _mm_set_ps(0.f, 0.f, 0.f, 1.f);
	V.row[1] = _mm_set_ps(0.f, 0.f, 1.f, 0.f);
	V.row[2] = _mm_set_ps(0.f, 1.f, 0.f, 0.f);
	V.row[3] = _mm_set_ps(0.f, 0.f, 0.f, 0.f);

	const __m128 sigma = svd_solve_sym(V, ATA);

	// A = UEV^T; U = A / (E*V^T)
	Mat4x4 Vinv;
	svd_pseudoinverse(Vinv, sigma, V);
	x = vec4_mul_m4x4(ATb, Vinv);
}


// ----------------------------------------------------------------------------

void qef_simd_add(
	const __m128& p, const __m128& n,
	Mat4x4& ATA,
	__m128& ATb,
	__m128& pointaccum)
{
	__m128 nX = _mm_mul_ps(_mm_shuffle_ps(n, n, _MM_SHUFFLE(0, 0, 0, 0)), n);
	__m128 nY = _mm_mul_ps(_mm_shuffle_ps(n, n, _MM_SHUFFLE(1, 1, 1, 1)), n);
	__m128 nZ = _mm_mul_ps(_mm_shuffle_ps(n, n, _MM_SHUFFLE(2, 2, 2, 2)), n);

	ATA.row[0] = _mm_add_ps(ATA.row[0], nX);
	ATA.row[1] = _mm_add_ps(ATA.row[1], nY);
	ATA.row[2] = _mm_add_ps(ATA.row[2], nZ);

	const float d = vec4_dot(p, n);
	__m128 x = _mm_set_ps(0.f, d, d, d);
	x = _mm_mul_ps(x, n);
	ATb = _mm_add_ps(ATb, x);
	pointaccum = _mm_add_ps(pointaccum, p);
}

// ----------------------------------------------------------------------------

float qef_simd_calc_error(const Mat4x4& A, const __m128& x, const __m128& b)
{
	__m128 tmp = vec4_mul_m4x4(x, A);
	tmp = _mm_sub_ps(b, tmp);

	return vec4_dot(tmp, tmp);
}

// ----------------------------------------------------------------------------

float qef_simd_solve(
	const Mat4x4& ATA,
	const __m128& ATb,
	const __m128& pointaccum,
	__m128& x)
{
	const __m128 masspoint = _mm_div_ps(pointaccum, _mm_set1_ps(pointaccum.m128_f32[3]));

	__m128 p = vec4_mul_m4x4(masspoint, ATA);
	p = _mm_sub_ps(ATb, p);

	svd_solve_ATA_ATb(ATA, p, x);

	const float error = qef_simd_calc_error(ATA, x, ATb);
	x = _mm_add_ps(x, masspoint);

	return error;
}

// ----------------------------------------------------------------------------

float qef_solve_from_points(
	const __m128* positions,
	const __m128* normals,
	const int count,
	__m128* solved_position)
{
	__m128 pointaccum = _mm_set1_ps(0.f);
	__m128 ATb = _mm_set1_ps(0.f);

	Mat4x4 ATA;
	ATA.row[0] = _mm_set1_ps(0.f);
	ATA.row[1] = _mm_set1_ps(0.f);
	ATA.row[2] = _mm_set1_ps(0.f);
	ATA.row[3] = _mm_set1_ps(0.f);

	for (int i = 0; i < count && i < QEF_MAX_INPUT_COUNT; i++)
	{
		qef_simd_add(positions[i], normals[i], ATA, ATb, pointaccum);
	}

	__declspec(align(16)) float x[4];
	_mm_store_ps(x, ATb);
	_mm_set_ps(0.f, x[2], x[1], x[0]);

	return qef_simd_solve(ATA, ATb, pointaccum, *solved_position);
}

// ----------------------------------------------------------------------------

float qef_solve_from_points_4d(
	const float* positions,
	const float* normals,
	const int count,
	float* solved_position)
{
	if (count < 2 || count > QEF_MAX_INPUT_COUNT)
	{
		solved_position[0] = solved_position[1] = solved_position[2] = solved_position[3] = 0.f;
		return 0.f;
	}

	__m128 p[QEF_MAX_INPUT_COUNT];
	__m128 n[QEF_MAX_INPUT_COUNT];
	for (int i = 0; i < count; i++)
	{
		p[i] = _mm_load_ps(&positions[i * 4]);
		n[i] = _mm_load_ps(&normals[i * 4]);
	}

	__m128 solved;
	const float error = qef_solve_from_points(p, n, count, &solved);
	_mm_store_ps(solved_position, solved);
	return error;
}

// ----------------------------------------------------------------------------

float qef_solve_from_points_4d_interleaved(
	const float* data,
	const size_t stride,
	const int count,
	float* solved_position)
{
	if (count < 2 || count > QEF_MAX_INPUT_COUNT)
	{
		solved_position[0] = solved_position[1] = solved_position[2] = solved_position[3] = 0.f;
		return 0.f;
	}

	__m128 p[QEF_MAX_INPUT_COUNT];
	__m128 n[QEF_MAX_INPUT_COUNT];
	for (int i = 0; i < count; i++)
	{
		p[i] = _mm_load_ps(&data[(i * stride) + 0]);
		n[i] = _mm_load_ps(&data[(i * stride) + 4]);
	}

	__m128 solved;
	const float error = qef_solve_from_points(p, n, count, &solved);
	_mm_store_ps(solved_position, solved);
	return error;
}

// ----------------------------------------------------------------------------

float qef_solve_from_points_3d(
	const float* positions,
	const float* normals,
	const int count,
	float* solved_position)
{
	if (count < 2 || count > QEF_MAX_INPUT_COUNT)
	{
		solved_position[0] = solved_position[1] = solved_position[2] = solved_position[3] = 0.f;
		return 0.f;
	}

	__m128 p[QEF_MAX_INPUT_COUNT];
	__m128 n[QEF_MAX_INPUT_COUNT];
	for (int i = 0; i < count; i++)
	{
		const float* pos = &positions[i * 3];
		const float* nrm = &normals[i * 3];
		p[i] = _mm_set_ps(1.f, pos[2], pos[1], pos[0]);
		n[i] = _mm_set_ps(0.f, nrm[2], nrm[1], nrm[0]);
	}

	__m128 solved;
	const float error = qef_solve_from_points(p, n, count, &solved);

	solved_position[0] = solved.m128_f32[0];
	solved_position[1] = solved.m128_f32[1];
	solved_position[2] = solved.m128_f32[2];
	return error;
}



#endif // QEF_INCLUDE_IMPL


#endif	//	HAS_QEF_SIMD_H_BEEN_INCLUDED