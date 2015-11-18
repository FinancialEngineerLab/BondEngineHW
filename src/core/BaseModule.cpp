/*
 * BaseModule.cpp
 *
 *  Created on: Nov 11, 2015
 *      Author: Chandra
 */
#include <cassert>
#include <iostream>
#include <math.h>
#include "BaseModule.h"
#include "spline.h"
#include "Constants.h"

BaseModule::BaseModule() {
	// TODO Auto-generated constructor stub

}

BaseModule::~BaseModule() {
	// TODO Auto-generated destructor stub
}


void BaseModule::calculateEt(){
	data.Et.assign(data.time.size(), 0);
	data.Et[0] = exp(data.aMeanReversion[0] * data.time[0]);
	for(int i = 1; i<data.time.size(); ++i){
		data.Et[i]  = exp(data.aMeanReversion[i] * (data.time[i] - data.time[i-1])) * data.Et[i-1];
	}
}

double BaseModule::calculateB(double t, double T){
    int position_t = locate(t);
    int position_T = locate(T);
    double summation = 0;
    for (int i = position_t+1; i <= position_T; i++){
        summation = summation + (data.time[i] - data.time[i-1]) / data.Et[i];
    }
    if(t){
        summation = summation * data.Et[position_t];
    }
//    cout << "calculateB = " << summation << endl;
    return summation;
}


int BaseModule::locate(double t){
    //return index in vector for given value
//	std::cout << t <<" ";
	if (t==0)
		return -1;
	std::map<double,int>::iterator it;
	it = timePos.find(t);
	assert(it!=timePos.end());
//	std::cout << it->second << std::endl;
    return it->second;
}

double BaseModule::calculateVr(double t){
    //Vr always have s=0 in our Problem
    if (t == 0) {
        return 0;
    }
    int position_t = locate(t);
    double Vr = 0;
    for(int i = 0; i<=position_t; ++i){
        Vr += pow(data.Et[i], 2) * pow(data.sigma[i], 2) * (data.time[i] - data.time[i-1]);
    }
    Vr = Vr / pow(data.Et[position_t], 2);
//    cout << "calculateVr = \t" << Vr << endl;
    return Vr;
}

double BaseModule::calculateVp(double Tf, double Tp){
    //Vp always have t=0 in our Problem
	double t = calculateVr(Tf) * pow(calculateB(Tf, Tp), 2);
//	cout << "calculateVp = " << t << endl;
    return t;
}

double BaseModule::calculateA(double t, double T){
    int position_t = locate(t);
    int position_T = locate(T);
    double P0t=1;
    if(t){
        P0t=data.priceD[position_t];
        //f0t=data.f[position_t];
    }
    double valB = calculateB(t, T);
    double x = log(data.priceD[position_T] / P0t) ;
    double y = valB * data.forward[position_t+1];
    double z = 0.5 * pow(valB, 2) * calculateVr(t);
    double t1 = x + y - z;
//    cout << "calculateA = " << x << "+" << y << "-" << z << "=" << t1 << endl;
    return t1;
}

//-----------------------


// Returns the erf() of a value (not super precise, but ok)
double BaseModule::erf(double x){
    double y = 1.0 / ( 1.0 + 0.3275911 * x);
    return 1 - (((((
                    + 1.061405429  * y
                    - 1.453152027) * y
                   + 1.421413741) * y
                  - 0.284496736) * y
                 + 0.254829592) * y)
    * exp (-x * x);
}

// Returns the probability of [-inf,x] of a gaussian distribution
double BaseModule::N(double x, double mu, double sigma){
    return 0.5 * (1 + erf((x - mu) / (sigma * sqrt(2.))));
}

double BaseModule::ZBP(double Tf, double Tp, double X){
    int position_Tf = locate(Tf);
    int position_Tp = locate(Tp);
    double l = log(data.priceD[position_Tf] * X / data.priceD[position_Tp]);
    double v = sqrt(calculateVp(Tf, Tp));
    double dP =  l / v + 0.5 * v;
    double dN = l / v - 0.5 * v;
    return X * data.priceD[position_Tf] * N(dP, 0, 1) - data.priceD[position_Tp] * N(dN, 0, 1);
}

//f(r)
double BaseModule::fun_r(const vector<double>& c, const vector<double>& A, const vector<double>& B, double r){
    double ret = 0;
    for(int i = 0; i < c.size(); ++i){
        ret += c[i] * exp(A[i] - B[i] * r);
    }
    return ret - 1;
}

//f'(r)
double BaseModule::fun_dev(const vector<double>& c, const vector<double>& A, const vector<double>& B, double r){
    double d = 0;
    for(int i = 0; i < c.size(); ++i){
        d = d - c[i] * exp(A[i] - B[i] * r) * B[i];
    }
    return d;
}

//Newton's algorithm to solve r*
double BaseModule::solveR(const vector<double>& c, const vector<double>& A, const vector<double>& B){
    double r = 0.5;
    while (fabs(fun_r(c, A, B, r)) > 1E-10) {
        r = r - fun_r(c, A, B, r) / fun_dev(c, A, B, r);
    }
    return r;
}

double BaseModule::pSwaption(double K, double T0, double Tp){
    int position_T0 = locate(T0);
    int position_Tp = locate(Tp);
    int cashFlowTimes = position_Tp-position_T0;
    vector<double> ci(cashFlowTimes);
    vector<double> Ti(cashFlowTimes);
    vector<double> Ai(cashFlowTimes);
    vector<double> Bi(cashFlowTimes);
    for(int i = 0; i < cashFlowTimes; ++i){
        Ti[i] = data.time[position_T0 + i + 1];
        ci[i] = K * (Ti[i] - data.time[position_T0 + i]);
        Ai[i]=calculateA(T0, Ti[i]);
        Bi[i]=calculateB(T0, Ti[i]);
//        cout << "Ai=" << Ai[i] << " Bi=" << Bi[i] << " ci=" << ci[i] << " Ti=" << Ti[i] << endl;
    }
    ci.back() += 1;
    double r_star = solveR(ci, Ai, Bi);
    vector<double> Xi(cashFlowTimes);
    double price = 0;
    for(int i = 0; i < cashFlowTimes; ++i){
        Xi[i] = exp(Ai[i] - Bi[i] * r_star);
        price += ci[i] * ZBP(T0, Ti[i], Xi[i]);
//        cout << "Xi=" << Xi[i] << " price=" << price <<endl;
    }
//    cout << "FinalPrice=" <<"\t"<< price << "\t "<< T0 << " \t" << Tp << endl;
    return price;
}


double BaseModule::takeDev(const vector<double>& value, double point, int n){
    //take first or second derivative
    tk::spline s;
    s.set_points(data.time,value);
    if (n==1) {
        return (s(point+0.01)-s(point-0.01))/0.02;
    }
    else{
        return (s(point+0.01)-2*s(point)+s(point-0.01))/0.0001;
    }
}

vector<double> BaseModule::theta(){
    vector<double> V(data.time.size());
    vector<double> theta_vec(data.time.size());
    for(int i = 0; i < data.time.size(); ++i){
        vector<double> sig(i+1);
        for(int j=0; j<=i; j++){
            sig[j]=data.sigma[j] * calculateB(data.time[j], data.time[i]);
        }
        V[i]=pow(sig[0], 2) * data.time[0] ;
        for(int j = 1; j <= i; ++j){
            V[i]= V[i]+pow(sig[j], 2) * (data.time[j]-data.time[j-1]);
        }
    }
    for(int i = 0; i < data.time.size(); ++i){
        theta_vec[i]=takeDev(data.forward, data.time[i], 1)
        		+ data.aMeanReversion[i] * data.forward[i]
        		+ 0.5 * (takeDev(V, data.time[i], 2) + data.aMeanReversion[i] * takeDev(V, data.time[i], 1));
    }
    return theta_vec;
}
//
//double interval_bisection(double y_target,  // Target y value
//                          double m,         // Left interval value
//                          double n,         // Right interval value
//                          double epsilon,   // Tolerance
//                          T g) {            // Function object of type T, named g
//
//  // Create the initial x mid-point value
//  // Find the mapped y value of g(x)
//  double x = 0.5 * (m + n);
//  double y = g(x);
//
//  // While the difference between y and the y_target
//  // value is greater than epsilon, keep subdividing
//  // the interval into successively smaller halves
//  // and re-evaluate the new y.
//  do {
//    if (y < y_target) {
//      m = x;
//    }
//
//    if (y > y_target) {
//      n = x;
//    }
//
//    x = 0.5 * (m + n);
//    y = g(x);
//  } while (fabs(y-y_target) > epsilon);
//
//  return x;
//}

double BaseModule::calculateVswap(double T0, double Tn){
	double value=0.0;
	double Vp = calculateVp(T0, Tn);
	double priceT0 = data.priceD[locate(T0)];
	double priceTn = data.priceD[locate(Tn)];
	double val2 = pow((priceT0/(priceT0 - priceTn)),2.0);
	value = Vp * val2;
//	cout << "Vswap\t" << value << "\t" << priceT0 << "\t"
//			<< priceTn << "\tImplVol\t" << sqrt(value) << endl;
	return value;
}


void BaseModule::assignConstantMeanReversion(){
	double a = constants.FIXED_MEAN_REVERSION;
	int x = data.time.size();
	data.aMeanReversion.assign(x, a);
}

void BaseModule::assignConstantVol(){
	double s1 = constants.FIXED_VOLATILITY;
	int x = data.time.size();
	data.sigma.assign(x, s1);
}

double BaseModule::meanReversionLogisticFunction(double A0, double A1, double A2, double A3, double ti){
	return (A0 + (A1-A0)/(1+exp(A2*(A3-ti))));
}

double BaseModule::impliedVarianceRatios(double maturity, double tenor1, double tenor2){
	int maturityPos = locate(maturity);
	int tenorPos1 = locate(tenor1);
	int tenorPos2 = locate(tenor2);
	double pM = data.priceD[maturityPos];
	double pT1 = data.priceD[tenorPos1];
	double pT2 = data.priceD[tenorPos2];
	double b1 = calculateB(maturity, tenor1);
	double b2 = calculateB(maturity, tenor2);
	double num = (pM-pT2)*b1;
	double den = (pM-pT1)*b2;
	return pow((num/den), 2.0);
}

void BaseModule::initializeAndAssignConstantWeights(){
	int rows = actualVol.maturity.size();
	int cols = actualVol.tenor.size();
	for (int i=0; i<rows; i++){
		vector<double> rowi (cols, constants.CONSTANT_WEIGHT);
		actualVol.weights.push_back(rowi);
	}
}

void BaseModule::meanReversionCalibrationFunctionF(){
	int rows = actualVol.maturity.size();
	int cols = actualVol.tenor.size();
	vector<vector<double> > func;
	double funcTotal = 0.0;
	for (int i=0; i<rows; i++){
		vector<double> colsMinus1(cols-1);
		func.push_back(colsMinus1);
	}

	for (int i=0; i<rows; i++){
		for (int j=0; j<cols-1; j++){
			double volRatio = sqrt(impliedVarianceRatios(actualVol.maturity[i], actualVol.tenor[j], actualVol.tenor[j+1]));
			double weightRatio = actualVol.weights[i][j+1]/actualVol.weights[i][j];
			double impliedVolRatio = actualVol.vol[i][j+1]/actualVol.vol[i][j];
			func[i][j] = weightRatio * pow((volRatio-impliedVolRatio),2.0);
			funcTotal += func[i][j];
		}
	}
}

double BaseModule::strikeRateForSwaptionATM(double maturity, double tenor){
	int matPosition = locate(maturity);
	int tenPosition = locate(tenor);
	double priceAtMat = data.priceD[matPosition];
	double priceAtTen = data.priceD[tenPosition];
	int cashFlows = tenPosition - matPosition;
	double cashFlowsSum = 0.0;
	for(int i=matPosition+1; i<=tenPosition; i++){
		cashFlowsSum += constants.PAYMENT_FREQ*data.priceD[i];
	}
	return (data.priceD[matPosition]-data.priceD[tenPosition])/cashFlowsSum;
}

double BaseModule::blackFormula(double K, double F, double v, int w){

}
