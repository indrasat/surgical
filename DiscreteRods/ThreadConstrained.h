#include <stdlib.h>
#include <algorithm>

#ifdef MAC
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <GL/gle.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/gle.h>
#endif

#include <iostream>
#include <fstream>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <math.h>
#include "thread_discrete.h"

// import most common Eigen types
USING_PART_OF_NAMESPACE_EIGEN

class ThreadConstrained {
	public:
		ThreadConstrained(int vertices_num);
		int numVertices() { return num_vertices; }
		const double rest_length() const { return threads.front()->rest_length(); }
		void get_thread_data(vector<Vector3d> &absolute_points);
		void get_thread_data(vector<Vector3d> &absolute_points, vector<double> &absolute_twist_angles);
		void get_thread_data(vector<Vector3d> &absolute_points, vector<double> &absolute_twist_angles, vector<Matrix3d> &absolute_material_frames);
		void get_thread_data(vector<Vector3d> &absolute_points, vector<Matrix3d> &absolute_material_frames);
		// parameters have to be of the right size, i.e. threads.size()+1
		void getConstrainedTransforms(vector<Vector3d> &positions, vector<Matrix3d> &rotations);
		// parameters have to be of the right size, i.e. num_vertices.
		void getAllTransforms(vector<Vector3d> &positions, vector<Matrix3d> &rotations);
		// parameters have to be of the right size.
		void getConstrainedNormals(vector<Vector3d> &normals);
		void getConstrainedVerticesNums(vector<int> &vertices_num);
		void getConstrainedVertices(vector<Vector3d> &constrained_vertices);
		void getFreeVerticesNums(vector<int> &vertices_nums);
		void getFreeVertices(vector<Vector3d> &free_vertices);
		void getOperableVertices(vector<int> &operable_vertices_num, vector<bool> &constrained_or_free);
		Vector3d start_pos();
		Vector3d end_pos();
		Matrix3d start_rot();
		Matrix3d end_rot();
		void set_coeffs_normalized(double bend_coeff, double twist_coeff, double grav_coeff);
		void set_coeffs_normalized(const Matrix2d& bend_coeff, double twist_coeff, double grav_coeff);
		void minimize_energy();
		void updateConstraints (vector<Vector3d> poss, vector<Matrix3d> rots);
		void printConstraints();
		void addConstraint (int absolute_vertex_num);
		void removeConstraint (int absolute_vertex_num);
		// Returns the number of the vertex that is nearest to pos. The chosen vertex have to be in vertices but not in vertex_exception.
		int nearestVertex(Vector3d pos, vector<Vector3d> vertices, vector<int> vertex_exceptions);
		Vector3d position(int absolute_vertex_num);
		Matrix3d rotation(int absolute_vertex_num);

	private:
		int num_vertices;
		vector<Thread*> threads;
		vector<double> zero_angle;
		vector<Matrix3d> rot_diff_start, rot_diff_end;
    vector<int> constrained_vertices_nums;
		void intermediateRotation(Matrix3d &inter_rot, Matrix3d end_rot, Matrix3d start_rot);
		// Splits the thread threads[thread_num] into two threads, which are stored at threads[thread_num] and threads[thread_num+1].  Threads in threads that are stored after thread_num now have a new thread_num which is one unit more than before. The split is done at vertex vertex of thread[thread_num]
		void splitThread(int thread_num, int vertex_num);
		// Merges the threads threads[thread_num] and threads[thread_num+1] into one thread, which is stored at threads[thread_num]. Threads in threads that are stored after thread_num+1 now have a new thread_num which is one unit less than before.
		void mergeThread(int thread_num);
		// Returns the thread number that owns the absolute_vertex number. absolute_vertex must not be a constrained vertex
		int threadOwner(int absolute_vertex_num);
		// Returns the local vertex number (i.e. vertex number within a thread), given the absolute vertex number (i.e. vertex number within all vertices).
		int localVertex(int absolute_vertex_num);
};

// Invalidates (sets to -1) the elements of v at indices 1. Indices are specified by constraintsNums. Indices in constraintsNums have to be in the range of v.
void invalidateAroundConstraintsNums(vector<int> &v, vector<int> constraintsNums);
// Invalidates (sets to -1) the elements of v at indices i. Indices i are specified by constraintsNums. Indices in constraintsNums have to be in the range of v.
void invalidateConstraintsNums(vector<int> &v, vector<int> constraintsNums);
/*// Elements v[i-1], v[i] and v[i+1] are removed from v, where the indices i are specified in invalidElements. invalidElements have to be sorted. The difference between adjacent elements in invalidElements have to be of at least 3.
template<typename T>
void removeInvalidSortedElements(vector<T> &v, vector<int> invalidElements);
// Elements v[i-1] and v[i+1] are removed from v, where the indices i are specified in invalidElements. invalidElements have to be sorted. The difference between adjacent elements in invalidElements have to be of at least 3.
template<typename T>
void removeAdjacentInvalidSortedElements(vector<T> &v, vector<int> invalidElements);
// Elements v[i] are invalidated (set to -1), where the indices i are specified in invalidElements. invalidElements have to be sorted.
template<typename T>
void invalidateSortedElements(vector<T> &v, vector<int> invalidElements);*/
template<typename T>
void mapAdd (vector<T> &v, T num);
// Inserts an element at index index. The elements after index index are moved one position to the right in vector.
template<typename T>
void insertAt (vector<T> &v, T e, int index);
// Last element of v1 and first element of v2 are equal to v[index].
template<typename T>
void splitVector (vector<T> &v1, vector<T> &v2, vector<T> v, int index);
// Last element of v1 is discarded since it is assumed to be the same as the first element of v2.
template<typename T>
void mergeVector (vector<T> &v, vector<T> v1, vector<T> v2);
// Last element of a vector in vectors is discarded since it is assumed to be the same as the first element of the consecutive vector in vectors. The last vector in vectors is the exception.
template<typename T>
void mergeMultipleVector(vector<T> &v, vector<vector<T> > vectors);
// Returns the position where the element was inserted.
int insertSorted (vector<int> &v, int e);
//Returns the position where the element was removed. Element to remove has to be in vector.
int removeSorted (vector<int> &v, int e);
//Returns the position of the element to be found. Element to find has to be in vector.
int find(vector<int> v, int e);