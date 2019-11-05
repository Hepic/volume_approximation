// VolEsti (volume computation and sampling library)

// Copyright (c) 20012-2018 Vissarion Fisikopoulos
// Copyright (c) 2018 Apostolos Chalkis

//Contributed and/or modified by Apostolos Chalkis, as part of Google Summer of Code 2018 program.

// Licensed under GNU LGPL.3, see LICENCE file

#ifndef ROUNDING_H
#define ROUNDING_H


#include "khach.h"


// ----- ROUNDING ------ //
// main rounding function
template <class Polytope, class Point, class Parameters, typename NT>
std::pair <NT, NT> rounding_min_ellipsoid(Polytope &P , std::pair<Point,NT> InnerBall, Parameters &var) {

    typedef typename Polytope::MT 	MT;
    typedef typename Polytope::VT 	VT;
    typedef typename Parameters::RNGType RNGType;
    unsigned int n=var.n, walk_len=var.walk_steps, i, j = 0;
    Point c = InnerBall.first;
    NT radius = InnerBall.second;
    std::list<Point> randPoints; //ds for storing rand points
    if (!P.get_points_for_rounding(randPoints)) {  // If P is a V-polytope then it will store its vertices in randPoints
        // If P is not a V-Polytope or number_of_vertices>20*domension
        // 2. Generate the first random point in P
        // Perform random walk on random point in the Chebychev ball
        Point p = get_point_on_Dsphere<RNGType, Point>(n, radius);
        p = p + c;

        //use a large walk length e.g. 1000
        rand_point_generator(P, p, 1, 50*n, randPoints, var);
        // 3. Sample points from P
        unsigned int num_of_samples = 10*n;//this is the number of sample points will used to compute min_ellipoid
        randPoints.clear();
        rand_point_generator(P, p, num_of_samples, walk_len, randPoints, var);
        /*NT current_dist, max_dist;
        for(typename std::list<Point>::iterator pit=randPoints.begin(); pit!=randPoints.end(); ++pit){
            current_dist=(*pit-c).squared_length();
            if(current_dist>max_dist){
                max_dist=current_dist;
            }
        }
        max_dist=std::sqrt(max_dist);
        R=max_dist/radius;*/
    }

    // Store points in a matrix to call Khachiyan algorithm for the minimum volume enclosing ellipsoid
    boost::numeric::ublas::matrix<double> Ap(n,randPoints.size());
    typename std::list<Point>::iterator rpit=randPoints.begin();
    typename std::vector<NT>::iterator qit;
    for ( ; rpit!=randPoints.end(); rpit++, j++) {
        qit = (*rpit).iter_begin(); i=0;
        for ( ; qit!=(*rpit).iter_end(); qit++, i++){
            Ap(i,j)=double(*qit);
        }
    }
    boost::numeric::ublas::matrix<double> Q(n,n);
    boost::numeric::ublas::vector<double> c2(n);
    size_t w=1000;
    KhachiyanAlgo(Ap,0.01,w,Q,c2); // call Khachiyan algorithm

    MT E(n,n);
    VT e(n);

    //Get ellipsoid matrix and center as Eigen objects
    for(unsigned int i=0; i<n; i++){
        e(i)=NT(c2(i));
        for (unsigned int j=0; j<n; j++){
            E(i,j)=NT(Q(i,j));
        }
    }


    //Find the smallest and the largest axes of the elliposoid
    Eigen::EigenSolver<MT> eigensolver(E);
    NT rel = std::real(eigensolver.eigenvalues()[0]);
    NT Rel = std::real(eigensolver.eigenvalues()[0]);
    for(unsigned int i=1; i<n; i++){
        if(std::real(eigensolver.eigenvalues()[i])<rel) rel=std::real(eigensolver.eigenvalues()[i]);
        if(std::real(eigensolver.eigenvalues()[i])>Rel) Rel=std::real(eigensolver.eigenvalues()[i]);
    }

    Eigen::LLT<MT> lltOfA(E); // compute the Cholesky decomposition of E
    MT L = lltOfA.matrixL(); // retrieve factor L  in the decomposition

    //Shift polytope in order to contain the origin (center of the ellipsoid)
    P.shift(e);

    MT L_1 = L.inverse();
    P.linear_transformIt(L_1.transpose());

    return std::pair<NT, NT> (L_1.determinant(),rel/Rel);
}


template <class Polytope, class Point, class Parameters, typename NT>
void round_projection_of_poly(Polytope &P, Point &p, Parameters &var, NT &rand_value, NT &diam) {

    //typedef typename Polytope::NT 	NT;
    typedef typename Polytope::MT 	MT;
    typedef typename Polytope::VT 	VT;
    //typedef typename Polytope::PolytopePoint 	Point;

    unsigned int n=var.n, walk_len=5, i, j;
    std::list<Point> randPoints;
    rand_value = 1.0;
    //var.cdhr_walk = false;
    MT E(n,n);
    VT e(n);
    boost::numeric::ublas::matrix<double> Ap(n,100*n);
    NT max_diam = 0.0, diam_iter, ratio1 = 0.0;

    int count = 1;
    while (count < 5) {

        randPoints.clear();
        boundary_rand_point_generator(P, p, 50*n, walk_len, randPoints, var);

        typename std::list<Point>::iterator rpit=randPoints.begin();
        typename std::vector<NT>::iterator qit;
        j=0, i=0;
        for ( ; rpit!=randPoints.end(); rpit++, j++) {
            qit = (*rpit).iter_begin(); i=0;
            for ( ; qit!=(*rpit).iter_end(); qit++, i++){
                Ap(i,j)=double(*qit);
            }
        }
        boost::numeric::ublas::matrix<double> Q(n,n);
        boost::numeric::ublas::vector<double> c2(n);
        size_t w=1000;
        KhachiyanAlgo(Ap,0.01,w,Q,c2); // call Khachiyan algorithm



        //Get ellipsoid matrix and center as Eigen objects
        for(unsigned int i=0; i<n; i++){
            e(i)=NT(c2(i));
            for (unsigned int j=0; j<n; j++){
                E(i,j)=NT(Q(i,j));
            }
        }

        Eigen::SelfAdjointEigenSolver<MT> es(E);
        MT D = es.eigenvalues().asDiagonal();
        //std::cout<<"D = "<<D<<"\n"<<"D(n,n) = "<<D(n-1,n-1)<<" D(0,0) = "<<D(0,0)<<std::endl;
        std::cout<<"max/min = "<<D(n-1,n-1)/D(0,0)<<std::endl;
        if (D(n-1,n-1)/D(0,0)<ratio1/3.0 || count==2){
            std::cout<<"computing distances"<<std::endl;
            typename std::list<Point>::iterator rpit=randPoints.begin();
            typename std::list<Point>::iterator rpit2=randPoints.begin();
            j=0;
            //max_diam = 0.0;
            while(rpit!=randPoints.end()) {
                j++;
                if (j==1){
                    rpit++;
                    continue;
                }
                diam_iter = std::sqrt(((*rpit)-(*rpit2)).squared_length());
                if (max_diam < diam_iter) max_diam = diam_iter;
                rpit++;
                rpit2++;
            }
            std::cout<<"diam = "<<diam<<std::endl;
            diam = max_diam;
            return;
        }
        if (count == 1) ratio1 = D(n-1,n-1)/D(0,0);

        Eigen::LLT<MT> lltOfA(E); // compute the Cholesky decomposition of E
        MT L = lltOfA.matrixL(); // retrieve factor L  in the decomposition
        MT L_1 = L.inverse();



        rand_value *= L_1.determinant();
        P.linear_transformIt(L_1.transpose());
        count++;

    }


}


template <class Polytope>
void get_vpoly_center(Polytope &P) {

    typedef typename Polytope::NT 	NT;
    typedef typename Polytope::MT 	MT;
    typedef typename Polytope::VT 	VT;
    typedef typename Polytope::PolytopePoint 	Point;

    unsigned int n = P.dimension(), i, j = 0;

    std::list<Point> randPoints; //ds for storing rand points
    P.get_points_for_rounding(randPoints);

    boost::numeric::ublas::matrix<double> Ap(n,randPoints.size());
    typename std::list<Point>::iterator rpit=randPoints.begin();
    typename std::vector<NT>::iterator qit;
    for ( ; rpit!=randPoints.end(); rpit++, j++) {
        qit = (*rpit).iter_begin(); i=0;
        for ( ; qit!=(*rpit).iter_end(); qit++, i++){
            Ap(i,j)=double(*qit);
        }
    }
    boost::numeric::ublas::matrix<double> Q(n,n);
    boost::numeric::ublas::vector<double> c2(n);
    size_t w=1000;
    KhachiyanAlgo(Ap,0.01,w,Q,c2); // call Khachiyan algorithm

    MT E(n,n);
    VT e(n);

    //Get ellipsoid matrix and center as Eigen objects
    for(unsigned int i=0; i<n; i++){
        e(i)=NT(c2(i));
        for (unsigned int j=0; j<n; j++){
            E(i,j)=NT(Q(i,j));
        }
    }

    P.shift(e);

}


// -------- ROTATION ---------- //
/*template <class MT, class Polytope>
MT rotating(Polytope &P){

    typedef boost::mt19937    RNGType;
    //typedef typename Polytope::MT 	MT;

    boost::random::uniform_real_distribution<> urdist(-1.0, 1.0);
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    RNGType rng(seed);
    unsigned int n = P.dimension();

    // pick a random rotation
    MT R(n,n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            R(i,j) = urdist(rng);
        }
    }

    Eigen::JacobiSVD<MT> svd(R, Eigen::ComputeFullU | Eigen::ComputeFullV);

    // apply rotation to the polytope P
    P.linear_transformIt(svd.matrixU());

    return svd.matrixU().inverse();
}*/

#endif
