#pragma once

#include <utility>

#include "System.h"

class Task : public System, public FWork {
protected:
    // НУ
    InitConditions cond;

    // заданные функции для НУ
    // phi(x, y) = {phi[k](x, y) : k = 1,3 }
    vec2d<double> phi;

    vec2d<double> setConst(const vec3<double>& c) {
        vec2d<double> res(3);
        loop3(k) {
            res[k].assign(grid[0], vec(grid[1], c[k]));
        }

        return res;
    }

    vec2d<double> setCommonCos(const vec3<double>& m, size_t alpha, size_t beta) {
        vec2d<double> res(3, matr2d<double>(grid[0], vec(grid[1], 0.)));

        loop3(k) {
            for (size_t i = 0; i < grid[0]; i++) {
                for (size_t j = 0; j < grid[1]; j++) {
                    // res[k]_ij = M[k] * cos(pi * a * x_i) * cos(pi * b * y_j)
                    res[k][i][j] = m[k] * std::cos(pi * (double)alpha * nodes[0][i]) * std::cos(pi * (double)beta * nodes[1][j]);
                }
            }
        }

        return res;
    }

    void defineBorderCondPhase1() {
        size_t n1 = grid[0];
        size_t n2 = grid[1];

        // для слоя 1
        loop3(k) {
            for (size_t j = 0; j < n2; j++) {
                // u_0js+0.5 = u_1js+0.5
                uMid[k][0][j] = uMid[k][1][j];

                // u_n1js+0.5 = u_n1-1js+0.5
                uMid[k][n1 - 1][j] = uMid[k][n1 - 2][j];
            }
        }
    }

    void defineBorderCondPhase2() {
        size_t n1 = grid[0];
        size_t n2 = grid[1];

        // для слоя 2
        loop3(k) {
            for (size_t i = 0; i < n1; i++) {
                // u_i0s+1 = u_i1s+1
                uTop[k][i][0] = uTop[k][i][1];

                // u_in2s+1 = u_in2-1s+1
                uTop[k][i][n2 - 1] = uTop[k][i][n2 - 2];
            }
        }
    }

public:
    Task() = default;

    Task(InitConditions cond_, const Area& area_, const Grid& grid_, const Parameters& params_, const vec3<double>& lambda_, bool clear)
            : System(area_, grid_, params_, lambda_),
              FWork(clear),
              cond(std::move(cond_))
    {
        this->definePhi();
    }

    void defineInitCond() {
        // 0 слой - u[k](x, y, 0) = phi[k](x, y)
        uLow = phi;
        uMid = phi;
        uTop = phi;
    }

    void defineBorderCond(size_t phase) {
        // работаем только с ГУ 2ого рода
        if (phase == 0) {
            this->defineBorderCondPhase1();

        } else if (phase == 1) {
            this->defineBorderCondPhase2();

        } else {
            throw std::runtime_error(AppConstansts::ALARM_DEFINE_BORDER_COND);
        }
    }

    void definePhi() {
        phi.resize(3, matr2d<double>(grid[0], vec(grid[1], 0.)));

        auto setCos = [&](size_t alpha, size_t beta) {
            vec2d<double> res(3);

            vec2d<double> C = setConst(cond.getC());
            vec2d<double> M = setCommonCos(cond.getM(), alpha, beta);

            // C + M = C + M * cosX * cosY
            loop3(k) {
                res[k].assign(grid[0], vec(grid[1], 0.));
                for (size_t i = 0; i < grid[0]; i++) {
                    for (size_t j = 0; j < grid[1]; j++) {
                        res[k][i][j] = C[k][i][j] + M[k][i][j];
                    }
                }
            }

            return C;
        };

        // define type of init cond
        switch(cond.getType()) {
            case Type::Const:
                phi = setConst(cond.getC());
                break;

            case Type::CosX:
                phi = setCos(cond.getAlpha(), 0.);
                break;

            case Type::CosY:
                phi = setCos(0., cond.getBeta());
                break;

            case Type::CosXCosY:
                phi = setCos(cond.getAlpha(), cond.getBeta());
                break;

            default:
                throw std::runtime_error(AppConstansts::ALARM_DEFINE_PHI);
        }
    }

    vec3<double> compare() {
        vec3<double> res(3, 0.);
        loop3(k) {
            for (size_t i = 0; i < grid[0]; i++) {
                for (size_t j = 0; j < grid[1]; j++) {
                    res[k] = std::fmax(uTop[k][i][j] - uLow[k][i][j], res[k]);
                }
            }
        }

        return res;
    }

    void getNorm(size_t s) {
        if (s == grid[2] - 1) {

            std::cout << "\nN = " << s << "\n";

            size_t i = 1;
            std::cout << std::setprecision(5);
            loop3(j) {
                std::cout << "||u(" << i << ")[" << s << "] - u(" << i << ")[" << s << "]|| = " << this->compare()[j] << "\n";
                i++;
            } std::cout << "\n";
        }
    }

    void execute() {
        this->defineInitCond();

        // s = 0, m
        loop(s, grid[2]) {
            FWork::fwrite(uLow, s, AppConstansts::MAIN_LAYER);

            // проводим этапы - 1, 2
            loop(phase, 2) {
                this->defineF(phase);
                this->defineLayersParams(phase);
                this->executeSerialSweepLayers(phase);
                // this->executeParallelSweepLayers(phase);

                this->defineBorderCond(phase);
            }

            // this->getNorm(s);
            FWork::fwrite(uMid, s, AppConstansts::HALF_LAYER);

            uLow = uTop;
        }

//        FWork::fread(AppConstansts::HALF_LAYER);
//        FWork::fread(AppConstansts::MAIN_LAYER);
    }
};
