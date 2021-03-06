/****************************************************************************
 *
 * $Id: vpROSRobot.cpp 3530 2012-01-03 10:52:12Z fpasteau $
 *
 * This file is part of the ViSP software.
 * Copyright (C) 2005 - 2012 by INRIA. All rights reserved.
 * 
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * ("GPL") version 2 as published by the Free Software Foundation.
 * See the file LICENSE.txt at the root directory of this source
 * distribution for additional information about the GNU GPL.
 *
 * For using ViSP with software that can not be combined with the GNU
 * GPL, please contact INRIA about acquiring a ViSP Professional 
 * Edition License.
 *
 * See http://www.irisa.fr/lagadic/visp/visp.html for more information.
 * 
 * This software was developed at:
 * INRIA Rennes - Bretagne Atlantique
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * France
 * http://www.irisa.fr/lagadic
 *
 * If you have questions regarding the use of this file, please contact
 * INRIA at visp@inria.fr
 * 
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 * Description:
 * vpRobot implementation for ROS middleware
 *
 * Authors:
 * Fran�ois Pasteau
 *
 *****************************************************************************/


/*!
  \file vpROSRobot.cpp
  \brief class that defines a vpRobot to use with ROS
*/

#include <visp/vpHomogeneousMatrix.h>
#include <visp/vpRobotException.h>
#include <visp_ros/vpROSRobot.h>
#include <visp/vpDebug.h>
#include <iostream>
#include <ros/ros.h>
#include <ros/time.h>
#include <sstream>

/**
 * \def MIN(x,y)
 * \brief Minimum of x and y
 */
#define MIN(X,Y) ((X)>(Y)?(Y):(X))

/**
 * \def MAX(x,y)
 * \brief Maximum of x and y
 */
#define MAX(X,Y) ((X)<(Y)?(Y):(X))

/**
 * \def CLIP(X,A,B)
 * \brief Clip X value so that A<X<B
 */
#define CLIP(X,A,B) (MIN(MAX(X,A),B))



//! constructor
vpROSRobot::vpROSRobot():
    isInitialized(false),
    odom_mutex(true),
    q(0,0,0,1),
    p(0,0,0),
    _sec(0),
    _nsec(0),
    displacement(6),
    pose_prev(6),
    _master_uri("http://127.0.0.1:11311"),
    _topic_cmd("cmd_vel"),
    _topic_odom("odom"),
    _nodespace("")
{

}



//! destructor
vpROSRobot::~vpROSRobot()
{
    if(isInitialized){
        isInitialized = false;
        spinner->stop();
        delete spinner;
        delete n;
    }
}

/*!
  Basic initialisation

  \param argc, argv : parameters of the main function
  */
void vpROSRobot::init(int argc, char **argv)
{
    if(!isInitialized){
        if(!ros::isInitialized()) ros::init(argc, argv, "visp_node", ros::init_options::AnonymousName);
        n = new ros::NodeHandle;
        cmdvel = n->advertise<geometry_msgs::Twist>(_nodespace + _topic_cmd, 1);
        odom = n->subscribe(_nodespace + _topic_odom, 1, &vpROSRobot::odomCallback,this,ros::TransportHints().tcpNoDelay());
        spinner = new ros::AsyncSpinner(1);
        spinner->start();
        isInitialized = true;
    }
}

/*!
  Basic initialisation

  */
void vpROSRobot::init(){
    if(ros::isInitialized() && ros::master::getURI() != _master_uri){
        throw (vpRobotException(vpRobotException::constructionError,
                                       "ROS already initialised with a different master_URI (" + ros::master::getURI() +" != " + _master_uri + ")") );
    }
    if(!isInitialized){
        int argc = 2;
        char *argv[2];
        std::string exe = "ros.exe", arg1 = "__master:=";
        strcpy(argv[0], exe.c_str());
        arg1.append(_master_uri);
        strcpy(argv[1], arg1.c_str());
        init(argc, argv);
    }
}


/*!
  Set the velocity (frame has to be specified) that will be applied to the robot.

  \param frame : Control frame. For the moment, only vpRobot::REFERENCE_FRAME is implemented.

  \param vel : A 6 dimension vector that corresponds to the velocities to apply to the robot.

  \exception vpRobotException::wrongStateError : If the specified control frame is not supported.

  */
void vpROSRobot::setVelocity(const vpRobot::vpControlFrameType frame, const vpColVector &vel)
{
  geometry_msgs::Twist msg;
  if (frame == vpRobot::REFERENCE_FRAME)
  {
      msg.linear.x = vel[0];
      msg.linear.y = vel[1];
      msg.linear.z = vel[2];
      msg.angular.x = vel[3];
      msg.angular.y = vel[4];
      msg.angular.z = vel[5];
      cmdvel.publish(msg);
  }
  else
  {
    throw vpRobotException (vpRobotException::wrongStateError,
                            "Cannot send the robot velocity in the specified control frame");
  }
}


/*!
  Get the robot position (frame has to be specified).

  \param frame : Control frame. For the moment, only vpRobot::REFERENCE_FRAME is implemented.

  \param pose : A 6 dimension vector that corresponds to the position of the robot.

  \exception vpRobotException::wrongStateError : If the specified control frame is not supported.

  */
void vpROSRobot::getPosition(const vpRobot::vpControlFrameType frame, vpColVector &pose) {
      while(!odom_mutex);
      odom_mutex = false;
      if (frame == vpRobot::REFERENCE_FRAME)
      {
          pose.resize(6);
          pose[0] = p[0];
          pose[1] = p[1];
          pose[2] = p[2];
          vpRotationMatrix R(q);
          vpRxyzVector V(R);
          pose[3]=V[0];
          pose[4]=V[1];
          pose[5]=V[2];
       }
       else
       {
         throw vpRobotException (vpRobotException::wrongStateError,
                                "Cannot get the robot position in the specified control frame");
       }
       odom_mutex = true;
}


/*!
  Get the robot displacement (frame has to be specified).

  \param frame : Control frame. For the moment, only vpRobot::REFERENCE_FRAME is implemented.

  \param dis : A 6 dimension vector that corresponds to the displacement of the robot since the last call to the function.

  \param timestamp : timestamp of the last update of the displacement

  \exception vpRobotException::wrongStateError : If the specified control frame is not supported.

  */
  void vpROSRobot::getDisplacement(const vpRobot::vpControlFrameType frame, vpColVector &dis, struct timespec &timestamp){
      while(!odom_mutex);
      odom_mutex = false;     
      vpColVector pose_cur(displacement);
      timestamp.tv_sec = _sec;
      timestamp.tv_nsec = _nsec;
      odom_mutex = true;
      if(frame == vpRobot::REFERENCE_FRAME){
          dis = pose_cur - pose_prev;
          pose_prev = pose_cur;
      }
      else
      {
        throw vpRobotException (vpRobotException::wrongStateError,
                                "Cannot get robot displacement in the specified control frame");
      }
  }

  /*!
    Get the robot displacement (frame has to be specified).

    \param frame : Control frame. For the moment, only vpRobot::REFERENCE_FRAME is implemented.

    \param dis : A 6 dimension vector that corresponds to the displacement of the robot since the last call to the function.

    \exception vpRobotException::wrongStateError : If the specified control frame is not supported.

    */
  void vpROSRobot::getDisplacement(const vpRobot::vpControlFrameType frame, vpColVector &dis){
      struct timespec timestamp;
      getDisplacement(frame, dis, timestamp);
  }


void vpROSRobot::odomCallback(const nav_msgs::Odometry::ConstPtr& msg){
    while(!odom_mutex);
    odom_mutex = false;
    p.set(msg->pose.pose.position.x,msg->pose.pose.position.y,msg->pose.pose.position.z);
    q.set(msg->pose.pose.orientation.x,msg->pose.pose.orientation.y,msg->pose.pose.orientation.z,msg->pose.pose.orientation.w);
    
    if(_sec != 0 || _nsec != 0){
        double dt = ((double)msg->header.stamp.sec - (double)_sec) + ((double)msg->header.stamp.nsec - (double)_nsec) / 1000000000.0;
        displacement[0] += msg->twist.twist.linear.x * dt;
        displacement[1] += msg->twist.twist.linear.y * dt;
        displacement[2] += msg->twist.twist.linear.z * dt;
        displacement[3] += msg->twist.twist.angular.x * dt;
        displacement[4] += msg->twist.twist.angular.y * dt;
        displacement[5] += msg->twist.twist.angular.z * dt;
    }
    _sec = msg->header.stamp.sec;
    _nsec = msg->header.stamp.nsec;
    odom_mutex = true;
}


/*
 * Local variables:
 * c-basic-offset: 2
 * End:
 */
