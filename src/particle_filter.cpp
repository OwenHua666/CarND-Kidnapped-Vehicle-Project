/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"
#include "helper_functions.h"

using namespace std;
const double eps = 0.00001;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of
	//   x, y, theta and their uncertainties from GPS) and all weights to 1.
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

    num_particles = 105;

    // Set the size of the weight
    weights.resize(num_particles);
    default_random_engine gen;

    // Create Gaussian Distributions for x, y, theta
    normal_distribution<double> dist_x(x, std[0]);
    normal_distribution<double> dist_y(y, std[1]);
    normal_distribution<double> dist_theta(theta, std[2]);

    // Iteratively generate num_particles and assign the corresponding id, x, y, theta, and weight to the structure
    for (int i = 0; i < num_particles; i++)
    {
        Particle new_particle;
        new_particle.id = i;
        new_particle.x = dist_x(gen);
        new_particle.y = dist_y(gen);
        new_particle.theta = dist_theta(gen);
        new_particle.weight = 1.0;

        // Attach the particle to the list
        particles.push_back(new_particle);
    }

    is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	// Gaussian Noise
	default_random_engine gen;
	normal_distribution<double> noise_x(0.0, std_pos[0]);
	normal_distribution<double> noise_y(0.0, std_pos[1]);
	normal_distribution<double> noise_theta(0.0, std_pos[2]);

	for (int i=0; i < num_particles; i++){
        // Deal with near_zero yaw rate
        if (fabs(yaw_rate) < eps){
            particles[i].x += velocity * delta_t * cos(particles[i].theta);
            particles[i].y += velocity * delta_t * sin(particles[i].theta);
        }
        else{
            particles[i].x += velocity / yaw_rate * (sin(particles[i].theta + yaw_rate*delta_t) - sin(particles[i].theta));
            particles[i].y += velocity / yaw_rate * (cos(particles[i].theta) - cos(particles[i].theta + yaw_rate * delta_t));
            particles[i].theta += yaw_rate * delta_t;
        }
        // add noise
        particles[i].x += noise_x(gen);
        particles[i].y += noise_y(gen);
        particles[i].theta += noise_theta(gen);
	}

}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to
	//   implement this method and use it as a helper during the updateWeights phase.

    // loop through the observations
    for (unsigned int i = 0; i < observations.size(); i++) {
        // current observation
        LandmarkObs cur_ob = observations[i];

        // initiate min dist to max dist
        double min_dist = numeric_limits<double>::max();

        // initiate the landmark id
        int map_id = -1;

        // loop through the predictions
        for (unsigned int j = 0; j < predicted.size(); j++){

            // current prediction
            LandmarkObs cur_pre = predicted[j];

            // calculate the dist
            double cur_dist = dist(cur_ob.x, cur_ob.y, cur_pre.x, cur_pre.y);

            if (cur_dist < min_dist){
                min_dist = cur_dist;
                map_id = cur_pre.id;
            }
        }
        observations[i].id = map_id;
    }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html
	for (int i = 0; i < num_particles; i++){

        // retrieve pose
        double p_x = particles[i].x;
        double p_y = particles[i].y;
        double p_theta = particles[i].theta;

        // a vector holding all the predicted particles within the sensor range
        vector<LandmarkObs> predictions;

        // for each map landmark position
        for (unsigned int j=0; j < map_landmarks.landmark_list.size(); j++){
            float lm_x = map_landmarks.landmark_list[j].x_f;
            float lm_y = map_landmarks.landmark_list[j].y_f;
            int lm_id = map_landmarks.landmark_list[j].id_i;

            // consider a rectangular region which makes the computation faster
            if (fabs(lm_x - p_x) <= sensor_range && fabs(lm_y - p_y) <= sensor_range){
                predictions.push_back(LandmarkObs{lm_id, lm_x, lm_y});
            }
        }
        // transfer coordinates
        vector<LandmarkObs> transformed_obs;
        for (unsigned int j=0; j < observations.size(); j++){
            double t_x = cos(p_theta)*observations[j].x - sin(p_theta)*observations[j].y + p_x;
            double t_y = sin(p_theta)*observations[j].x + cos(p_theta)*observations[j].y + p_y;
            transformed_obs.push_back(LandmarkObs{observations[j].id, t_x, t_y});
        }

        // perform data association function
        dataAssociation(predictions, transformed_obs);

        // init weight
        particles[i].weight = 1.0;

        for (unsigned int j = 0; j < transformed_obs.size(); j++){
            double obs_x, obs_y, pre_x, pre_y;

            obs_x = transformed_obs[j].x;
            obs_y = transformed_obs[j].y;

            int associated_prediction = transformed_obs[j].id;

            // search for the x,y coords of the prediction associated with the current observations
            for (unsigned int k = 0; k < predictions.size(); k++){
                if (predictions[k].id == associated_prediction){
                    pre_x = predictions[k].x;
                    pre_y = predictions[k].y;
                }
            }

            // calculate weight for this observation with multivariate Gaussian
            double s_x = std_landmark[0];
            double s_y = std_landmark[1];
            double var_x = pow(s_x,2);
            double var_y = pow(s_y,2);
            double obs_w = (1/(2*M_PI*s_x*s_y)) * exp(-(pow(pre_x-obs_x,2)/(2*var_x)+(pow(pre_y - obs_y,2)/(2*var_y))));

            particles[i].weight *= obs_w;
        }
	}
}

void ParticleFilter::resample() {
  // TODO: Resample particles with replacement with probability proportional to their weight.
  // NOTE: You may find std::discrete_distribution helpful here.
  //   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

  vector<Particle> new_particles;

  // get all of the current weights
  vector<double> weights;
  for (int i = 0; i < num_particles; i++) {
    weights.push_back(particles[i].weight);
  }
  default_random_engine gen;
  // generate random starting index for resampling wheel
  uniform_int_distribution<int> uniintdist(0, num_particles-1);

  auto index = uniintdist(gen);

  // get max weight
  double max_weight = *max_element(weights.begin(), weights.end());

  // uniform random distribution [0.0, max_weight)
  uniform_real_distribution<double> unirealdist(0.0, max_weight);

  double beta = 0.0;

  // spin the resample wheel!
  for (int i = 0; i < num_particles; i++) {
    beta += unirealdist(gen) * 2.0;
    while (beta > weights[index]) {
      beta -= weights[index];
      index = (index + 1) % num_particles;
    }
    new_particles.push_back(particles[index]);
  }

  particles = new_particles;
}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations,
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations= associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
