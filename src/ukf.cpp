#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // initially set to false, set to true in first call of ProcessMeasurement
  is_initialized_ = false;
  
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // State dimension
  n_x_ = 5;

  // Augmented state dimension
  n_aug_ = 7;

  // Sigma point spreading parameter
  lambda_ = 3 - n_aug_;
  
  // initial state vector
  x_ = VectorXd(n_x_);

  // initial covariance matrix
  P_ = MatrixXd(n_x_,n_x_);
  
  // Weights of sigma points
  weights_ = VectorXd(2*n_aug_+1);
  weights_(0) = lambda_/(lambda_+n_aug_);
  for (int i=1; i<2*n_aug_+1; i++)
  {
      weights_(i) = 0.5/(lambda_+n_aug_);
  }

  // initial predicted sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_,2*n_aug_+1);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 3;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  
  /*****************************************************************************
   *  Initialization
   ****************************************************************************/
   
  if (!is_initialized_) {
    // first measurement
    cout << "EKF: " << endl;
    x_.fill(0.0);

    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      /**
      Convert radar from polar to cartesian coordinates and initialize state.
      */
	  float ro = meas_package.raw_measurements_[0];
	  float theta = meas_package.raw_measurements_[1];
	  float ro_dot = meas_package.raw_measurements_[2];
	  x_(0) = ro*cos(theta);
	  x_(1) = ro*sin(theta);
	  x_(2) = ro_dot;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      /**
      Initialize state.
      */
	  float px = meas_package.raw_measurements_[0];
	  float py = meas_package.raw_measurements_[1];
	  x_(0) = px;
	  x_(1) = py;
	}
	
	//state covariance matrix P
	P_.fill(0.0);
	for (int i=0; i<n_x_; i++)
	{
		P_(i,i) = 1;
	}
			  
	time_us_ = meas_package.timestamp_;

	// done initializing, no need to predict or update
    is_initialized_ = true;
	cout << "initialized: " << is_initialized_ << endl;
    return;
  }
  
  //compute the time elapsed between the current and previous measurements
   double dt = (meas_package.timestamp_ - time_us_) / 1000000.0;	//dt - expressed in seconds
   time_us_ = meas_package.timestamp_;
  
  /*****************************************************************************
   *  Predict State
   ****************************************************************************/
   
  Prediction(dt);
  cout << "Prediction done" << endl;
  
  /*****************************************************************************
   *  Update State
   ****************************************************************************/
   
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_ == true)
  {
	  UpdateRadar(meas_package);
  }
  else if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_ == true)
  {
	  UpdateLidar(meas_package);
  }
  
  // print the output
  cout << "x_ = " << x_ << endl;
  cout << "P_ = " << P_ << endl;
  
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  
  /*****************************************************************************
   *  Generate Augmented State Sigma Points
   ****************************************************************************/
  
  //initialize augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);
  x_aug.fill(0.0);
  
  //initialize augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_,n_aug_);
  P_aug.fill(0.0);
  
  //initialize sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_,2*n_aug_+1);
  
  //create augmented mean state
  x_aug.head(n_x_) = x_;
  
  //create augmented covariance matrix
  P_aug.topLeftCorner(n_x_,n_x_) = P_;
  P_aug(5,5) = pow(std_a_,2);
  P_aug(6,6) = pow(std_yawdd_,2);
  
  //create square root matrix
  MatrixXd L = P_aug.llt().matrixL();
  
  //create augmented sigma points
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i < n_aug_; i++)
  {
    Xsig_aug.col(i+1)     = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }
  
  /*****************************************************************************
   *  Predict Augmented State Sigma Points Using CRTV Model
   ****************************************************************************/
  
  for (int i=0; i<2*n_aug_+1; i++)
  {
      float px = Xsig_aug(0,i);
      float py = Xsig_aug(1,i);
      float v = Xsig_aug(2,i);
      float yaw = Xsig_aug(3,i);
      float yaw_dot = Xsig_aug(4,i);
      float nu_a = Xsig_aug(5,i);
      float nu_yaw_dd = Xsig_aug(6,i);

      //predict sigma points
      float px_pred, py_pred;
      
      //avoid division by zero
      if (fabs(yaw_dot) < 0.001)
      {
          px_pred = v*cos(yaw)*delta_t;
          py_pred = v*sin(yaw)*delta_t;
      }
      else
      {
          px_pred = v/yaw_dot*(sin(yaw+yaw_dot*delta_t) - sin(yaw));
          py_pred = v/yaw_dot*(cos(yaw) - cos(yaw+yaw_dot*delta_t));
      }
      
      //write predicted sigma points into right column
      VectorXd pred_state = VectorXd(n_x_);
      pred_state << px + px_pred + 0.5*pow(delta_t,2)*cos(yaw)*nu_a,
                    py + py_pred + 0.5*pow(delta_t,2)*sin(yaw)*nu_a,
                    v + delta_t*nu_a,
                    yaw + yaw_dot*delta_t + 0.5*pow(delta_t,2)*nu_yaw_dd,
                    yaw_dot + delta_t*nu_yaw_dd;
      Xsig_pred_.col(i) = pred_state;
  }
  
  /*****************************************************************************
   *  Predict Mean and Covariance for Sigma Points
   ****************************************************************************/
   
  //predict state mean
  x_.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
      x_ += weights_(i)*Xsig_pred_.col(i);
  }

  //predict state covariance matrix
  P_.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
      //state difference
	  VectorXd x_diff = Xsig_pred_.col(i)-x_;
      //angle normalization
      NormalizeAngle(&(x_diff(3)));
	  P_ += weights_(i)*x_diff*x_diff.transpose();
  }
  
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  
  /*****************************************************************************
   *  Update Prediction Using Lidar Measurement
   ****************************************************************************/
   
  //set measurement dimension, lidar can measure px and pyt
  int n_z = 2;
  
  //create vector for incoming radar measurement
  VectorXd z = VectorXd(n_z);
  z = meas_package.raw_measurements_;
  
  //initialize matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z,2*n_aug_+1);
  
  //initialize mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  
  //initialize measurement covariance matrix
  MatrixXd S = MatrixXd(n_z,n_z);
  
  //initialize measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);

  //transform sigma points into measurement space
  for (int i=0; i<2*n_aug_+1; i++)
  {
      Zsig(0,i) = Xsig_pred_(0,i);
      Zsig(1,i) = Xsig_pred_(1,i);
  }
  
  //calculate mean predicted measurement
  z_pred.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
      z_pred += weights_(i)*Zsig.col(i);
  }
  
  //calculate innovation covariance matrix
  S.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
      //residual
      VectorXd z_diff = Zsig.col(i)-z_pred;
	  //angle normalization
      NormalizeAngle(&(z_diff(1)));
	  S += weights_(i)*z_diff*z_diff.transpose();
  }
  R << pow(std_laspx_,2), 0,
        0, pow(std_laspy_,2);
  S = S + R;
  
  //initialize cross correlation matrix
  MatrixXd Tc = MatrixXd(n_x_,n_z);
  
  //initialize kalman gain matrix
  MatrixXd K = MatrixXd(n_x_,n_z);
  
  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
      //state difference
	  VectorXd x_diff = Xsig_pred_.col(i)-x_;
      //angle normalization
      NormalizeAngle(&(x_diff(3)));
	  VectorXd z_diff = Zsig.col(i)-z_pred;
	  //angle normalization
      NormalizeAngle(&(z_diff(1)));
	  Tc += weights_(i)*x_diff*z_diff.transpose();
  }
  
  //calculate Kalman gain matrix;
  K = Tc*S.inverse();
  
  //update state mean and covariance matrix
  x_ += K*(z-z_pred);
  P_ -= K*S*K.transpose();
  
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  
  /*****************************************************************************
   *  Update Prediction Using Radar Measurement
   ****************************************************************************/
   
  //set measurement dimension, radar can measure ro, phi, and ro_dot
  int n_z = 3;
  
  //create vector for incoming radar measurement
  VectorXd z = VectorXd(n_z);
  z = meas_package.raw_measurements_;
  
  //initialize matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z,2*n_aug_+1);

  //initialize mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  
  //initialize measurement covariance matrix
  MatrixXd S = MatrixXd(n_z,n_z);
  
  //initialize measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);

  //transform sigma points into measurement space
  for (int i=0; i<2*n_aug_+1; i++)
  {
      float px = Xsig_pred_(0,i);
      float py = Xsig_pred_(1,i);
      float v = Xsig_pred_(2,i);
      float yaw = Xsig_pred_(3,i);
      Zsig(0,i) = sqrt(pow(px,2)+pow(py,2));
      Zsig(1,i) = atan2(py,px);
      if (Zsig(0,i) == 0)
      {
          Zsig(2,i) = 0;
      }
      else
      {
          Zsig(2,i) = (px*cos(yaw)*v + py*sin(yaw)*v)/Zsig(0,i);
      }
  }
  
  //calculate mean predicted measurement
  z_pred.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
      z_pred += weights_(i)*Zsig.col(i);
  }
  
  //calculate innovation covariance matrix
  S.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
	  //residual
      VectorXd z_diff = Zsig.col(i)-z_pred;
      //angle normalization
      NormalizeAngle(&(z_diff(1)));
      S += weights_(i)*z_diff*z_diff.transpose();
  }
  R << pow(std_radr_,2), 0, 0,
        0, pow(std_radphi_,2), 0,
        0, 0, pow(std_radrd_,2);
  S = S + R;
  
  //initialize cross correlation matrix
  MatrixXd Tc = MatrixXd(n_x_,n_z);
  
  //initialize kalman gain matrix
  MatrixXd K = MatrixXd(n_x_,n_z);
  
  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++)
  {
      //state difference
	  VectorXd x_diff = Xsig_pred_.col(i)-x_;
      //angle normalization
      NormalizeAngle(&(x_diff(3)));
	  VectorXd z_diff = Zsig.col(i)-z_pred;
	  //angle normalization
      NormalizeAngle(&(z_diff(1)));
	  Tc += weights_(i)*x_diff*z_diff.transpose();
  }
  
  //calculate Kalman gain matrix;
  K = Tc*S.inverse();
  
  //residual
  VectorXd z_diff = z-z_pred;
  //angle normalization
  NormalizeAngle(&(z_diff(1)));
  
  //update state mean and covariance matrix
  x_ += K*z_diff;
  P_ -= K*S*K.transpose();
  
}

/**
   * Normalize the angular value between -PI and PI
   */
  void UKF::NormalizeAngle(double *angle) {

  while (*angle > M_PI)
  {
	  *angle -= 2.*M_PI;
  }
  
  while (*angle < -M_PI)
  {
	  *angle += 2.*M_PI;
  }
  
}
