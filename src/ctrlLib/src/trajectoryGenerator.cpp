
#include <yarp/math/Math.h>
#include <iCub/trajectoryGenerator.h>

using namespace std;
using namespace yarp;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;
using namespace ctrl;


/************************************************************************/
minJerkTrajGen::minJerkTrajGen(const double _Ts, const Vector &x0) :
                               Ts(_Ts), x(x0)
{
    dim=x.length();
    xdOld=x;

    v.resize(dim,0.0);
    a.resize(dim,0.0);
    vtau.resize(6);
    vData.resize(5);
    aData.resize(4);

    for (unsigned int i=0; i<dim; i++)
    {
        Vector c(6); c=0.0;
        coeff.push_back(c);
    }

    TOld=t0=t1=t=0.0;

    vtau[0]=1.0;

    vData[0]=1.0;
    vData[1]=2.0;
    vData[2]=3.0;
    vData[3]=4.0;
    vData[4]=5.0;

    aData[0]=2.0;
    aData[1]=6.0;
    aData[2]=12.0;
    aData[3]=20.0;

    mutex=new Semaphore(1);

    state=MINJERK_STATE_REACHED;
}


/************************************************************************/
double minJerkTrajGen::calcTau(const double T,  const double dt)
{
    t1=t;

    if (dt<0.0)
        t+=Ts;
    else
        t+=dt;        

    double tau=(t-t1)/T;
    if (tau>1.0)
        tau=1.0;

    return tau;
}


/************************************************************************/
void minJerkTrajGen::calcCoeff(const double T, const Vector &xd, const Vector &fb)
{
    for (unsigned int i=0; i<dim; i++)
    {
        double ei=xd[i]-fb[i];
        double tmp1=T*v[i];
        double tmp2=T*T*a[i]/2.0;
    
        coeff[i][0]=fb[i];
        coeff[i][1]=tmp1;
        coeff[i][2]=tmp2;
        coeff[i][3]=-3.0*tmp2-6.0*tmp1+10.0*ei;
        coeff[i][4]= 3.0*tmp2+8.0*tmp1-15.0*ei;
        coeff[i][5]=-tmp2-3.0*tmp1+6.0*ei;
    }
}


/************************************************************************/
void minJerkTrajGen::compute(const double T, const Vector &xd, const Vector &fb,
                             const double tol, const double dt)
{
    double tau=calcTau(T,dt);

    if ((T!=TOld) || !(xd==xdOld))
    {   
        tau=calcTau(T,dt);
        t0=t;

        xdOld=xd;
        TOld=T;
        state=MINJERK_STATE_RUNNING;
    }

    if (norm(xd-fb)<tol)
    {
        x=xd;
        v=a=0.0;
        state=MINJERK_STATE_REACHED;
    }
    else if (state==MINJERK_STATE_REACHED || t-t0>=T)
    {
        tau=calcTau(T,dt);
        t0=t;

        state=MINJERK_STATE_RUNNING;
    }

    calcCoeff(T,xd,fb);

    if (state==MINJERK_STATE_RUNNING)
    {
        for (int j=1; j<vtau.length(); j++)
            vtau[j]=tau*vtau[j-1];

        mutex->wait();
        for (unsigned int i=0; i<dim; i++)
        {    
            x[i]=yarp::math::dot(coeff[i],vtau);

            v[i]=a[i]=0.0;
            for (int j=0; j<vData.length(); j++)
            {    
                v[i]+=coeff[i][j+1]*vData[j]/T*vtau[j];

                if (j<4)
                    a[i]+=coeff[i][j+2]*aData[j]/(T*T)*vtau[j];
            }            
        }
        mutex->post();
    }
}


/************************************************************************/
Vector minJerkTrajGen::get_x()
{
    mutex->wait();
    Vector latch_x=x;
    mutex->post();

    return latch_x;
}


/************************************************************************/
Vector minJerkTrajGen::get_v()
{
    mutex->wait();
    Vector latch_v=v;
    mutex->post();

    return latch_v;
}


/************************************************************************/
Vector minJerkTrajGen::get_a()
{
    mutex->wait();
    Vector latch_a=a;
    mutex->post();

    return latch_a;
}


/************************************************************************/
minJerkTrajGen::~minJerkTrajGen()
{
    delete mutex;
    coeff.clear();
}




