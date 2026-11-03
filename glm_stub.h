#pragma once
#include <cmath>
#include <cstring>

namespace glm_stub {
	const float PI = 3.14159265358979323846f;
	inline float radians(float deg) { return deg * (PI / 180.0f); }

	struct vec3 { float x, y, z; vec3() : x(0), y(0), z(0) {} vec3(float a, float b, float c): x(a), y(b), z(c) {} vec3(float v): x(v), y(v), z(v) {} };

	struct mat4 {
		float m[16];
		mat4() { // identity
			for (int i = 0; i < 16; ++i) m[i] = 0.0f;
			m[0] = m[5] = m[10] = m[15] = 1.0f;
		}
		// constructor que inicializa la diagonal con un valor
		mat4(float diag) { for (int i = 0; i < 16; ++i) m[i] = 0.0f; m[0]=m[5]=m[10]=m[15]=diag; }
	};

	inline const float* value_ptr(const mat4 &mat) { return mat.m; }

	inline mat4 operator*(const mat4 &a, const mat4 &b) {
		mat4 r;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k) sum += a.m[i*4 + k] * b.m[k*4 + j];
				r.m[i*4 + j] = sum;
			}
		}
		return r;
	}

	inline mat4 translate(mat4 m, const vec3 &v) {
		mat4 r = m;
		r.m[12] = m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z + m.m[12];
		r.m[13] = m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z + m.m[13];
		r.m[14] = m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14];
		r.m[15] = m.m[3]*v.x + m.m[7]*v.y + m.m[11]*v.z + m.m[15];
		return r;
	}

	inline mat4 scale(mat4 m, const vec3 &v) {
		mat4 r;
		r.m[0] = m.m[0] * v.x; r.m[1] = m.m[1] * v.x; r.m[2] = m.m[2] * v.x; r.m[3] = m.m[3] * v.x;
		r.m[4] = m.m[4] * v.y; r.m[5] = m.m[5] * v.y; r.m[6] = m.m[6] * v.y; r.m[7] = m.m[7] * v.y;
		r.m[8] = m.m[8] * v.z; r.m[9] = m.m[9] * v.z; r.m[10] = m.m[10] * v.z; r.m[11] = m.m[11] * v.z;
		r.m[12] = m.m[12]; r.m[13] = m.m[13]; r.m[14] = m.m[14]; r.m[15] = m.m[15];
		return r;
	}

	inline mat4 perspective(float fovy, float aspect, float znear, float zfar) {
		float f = 1.0f / tanf(fovy / 2.0f);
		mat4 r; memset(r.m, 0, sizeof(r.m));
		r.m[0] = f / aspect;
		r.m[5] = f;
		r.m[10] = (zfar + znear) / (znear - zfar);
		r.m[11] = -1.0f;
		r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
		return r;
	}

	inline vec3 normalize(const vec3 &v) {
		float l = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
		if (l == 0) return vec3(0,0,0);
		return vec3(v.x/l, v.y/l, v.z/l);
	}

	inline vec3 cross(const vec3 &a, const vec3 &b) {
		return vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
	}

	inline float dot(const vec3 &a, const vec3 &b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

	inline mat4 lookAt(const vec3 &eye, const vec3 &center, const vec3 &up) {
		vec3 f = normalize(vec3(center.x - eye.x, center.y - eye.y, center.z - eye.z));
		vec3 s = normalize(cross(f, up));
		vec3 u = cross(s, f);
		mat4 r; memset(r.m, 0, sizeof(r.m));
		r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
		r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
		r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
		r.m[15] = 1.0f;
		// translation
		mat4 t;
		t.m[12] = -eye.x; t.m[13] = -eye.y; t.m[14] = -eye.z;
		return r * t;
	}

	inline mat4 rotate(mat4 m, float angle, const vec3 &axis) {
		float c = cosf(angle);
		float s = sinf(angle);
		vec3 a = normalize(axis);
		float x = a.x, y = a.y, z = a.z;
		mat4 r; memset(r.m, 0, sizeof(r.m));
		r.m[0] = c + (1 - c) * x * x;
		r.m[1] = (1 - c) * x * y + s * z;
		r.m[2] = (1 - c) * x * z - s * y;
		r.m[4] = (1 - c) * y * x - s * z;
		r.m[5] = c + (1 - c) * y * y;
		r.m[6] = (1 - c) * y * z + s * x;
		r.m[8] = (1 - c) * z * x + s * y;
		r.m[9] = (1 - c) * z * y - s * x;
		r.m[10] = c + (1 - c) * z * z;
		r.m[15] = 1.0f;
		return m * r;
	}

} // namespace glm_stub

// Mapar nombres a "glm" para que el código use glm::...
namespace glm {
	using glm_stub::mat4;
	using glm_stub::vec3;
	using glm_stub::value_ptr;
	inline mat4 perspective(float fovy, float aspect, float znear, float zfar) { return glm_stub::perspective(fovy, aspect, znear, zfar); }
	inline mat4 lookAt(const vec3 &eye, const vec3 &center, const vec3 &up) { return glm_stub::lookAt(eye, center, up); }
	inline mat4 translate(mat4 m, const vec3 &v) { return glm_stub::translate(m, v); }
	inline mat4 rotate(mat4 m, float angle, const vec3 &axis) { return glm_stub::rotate(m, angle, axis); }
	inline mat4 scale(mat4 m, const vec3 &v) { return glm_stub::scale(m, v); }
	inline float radians(float deg) { return glm_stub::radians(deg); }
}
