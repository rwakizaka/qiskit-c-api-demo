/*
# This code is part of Qiskit.
#
# (C) Copyright IBM 2025.
#
# This code is licensed under the Apache License, Version 2.0. You may
# obtain a copy of this license in the LICENSE.txt file in the root directory
# of this source tree or at http://www.apache.org/licenses/LICENSE-2.0.
#
# Any modifications or derivative works of this code must retain this
# copyright notice, and modified files need to carry a notice indicating
# that they have been altered from the originals.
*/

#ifndef SBD_HELPER_HPP_
#define SBD_HELPER_HPP_

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>

#ifdef _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#endif

#define USE_MATH_DEFINES
#include <cmath>

#include "mpi.h"
#include "sbd/sbd.h"

struct SBD {
    int task_comm_size = 1;
    int adet_comm_size = 1;
    int bdet_comm_size = 1;
    int h_comm_size = 1;

    int max_it = 1;
    int max_nb = 10;
    double eps = 1.0e-12;
    double max_time = 600.0;
    int init = 0;

    double threshold = 0.0;

    // This default value is for the Fe4S4
    double energy_target = -326.6;
    double energy_variance = 1.0;

    std::string adetfile = "AlphaDets.bin";
    std::string fcidumpfile = "";
};

SBD generate_sbd_data(int argc, char *argv[])
{
    SBD sbd;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--fcidump") {
            sbd.fcidumpfile = std::string(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--iteration") {
            sbd.max_it = std::atoi(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--block") {
            sbd.max_nb = std::atoi(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--tolerance") {
            sbd.eps = std::atof(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--max_time") {
            sbd.max_time = std::atof(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--adet_comm_size") {
            sbd.adet_comm_size = std::atoi(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--bdet_comm_size") {
            sbd.bdet_comm_size = std::atoi(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--task_comm_size") {
            sbd.task_comm_size = std::atoi(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--energy_target") {
            sbd.init = std::atoi(argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "--energy_variance") {
            sbd.init = std::atoi(argv[i + 1]);
            i++;
        }
    }
    return sbd;
}

// energy, occupancy
std::tuple<double, std::vector<double>>
sbd_main(const MPI_Comm &comm, const SBD &sbd_data)
{
    const int master_rank = 0;
    int mpi_rank, mpi_size;
    MPI_Comm_rank(comm, &mpi_rank);
    MPI_Comm_size(comm, &mpi_size);

    if (mpi_rank == master_rank) {
        if (sbd_data.adetfile.empty()) {
            throw std::runtime_error("adetfile is not set.");
        }
        if (sbd_data.bdetfile.empty()) {
            throw std::runtime_error("bdetfile is not set.");
        }
    }

    /* ========================================================================
     * Loading problem (fcidump)
     */

    sbd::FCIDump fcidump;
    if (mpi_rank == master_rank) {
        fcidump = sbd::LoadFCIDump(sbd_data.fcidump_file);
    }
    sbd::MpiBcast(fcidump, master_rank, comm);

    int L; // Number of orbitals
    int N; // Number of electrons
    for (const auto &[key, value] : fcidump.header) {
        if (key == std::string("NORB")) {
            L = std::atoi(value.c_str());
        }
        if (key == std::string("NELEC")) {
            N = std::atoi(value.c_str());
        }
    }

    /* ========================================================================
     * Setup determinants for alpha and beta spin orbitals
     */
    std::vector<std::vector<size_t>> adet, bdet;
    if (mpi_rank == master_rank) {
        sbd::LoadAlphaDets(sbd_data.adetfile, adet, sbd_data.inner.bit_length, L);
        sbd::sort_bitarray(adet);
        sbd::LoadAlphaDets(sbd_data.bdetfile, bdet, sbd_data.inner.bit_length, L);
        sbd::sort_bitarray(bdet);
    }
    if (sbd_data.inner.do_shuffle != 0) {
        if (mpi_rank == master_rank) {
            unsigned int taxi = 1729;
            unsigned int magic = 137;
            sbd::ShuffleDet(adet, taxi);
            sbd::ShuffleDet(bdet, magic);
        }
    }
    sbd::MpiBcast(adet, master_rank, comm);
    sbd::MpiBcast(bdet, master_rank, comm);

    /* ========================================================================
     * Sample-based diagonalization using data for fcidump, adet, bdet.
     */
    const std::string loadname = "";
    const std::string savename = "";
    double energy;
    std::vector<double> density;
    std::vector<std::vector<size_t>> co_adet;
    std::vector<std::vector<size_t>> co_bdet;
    std::vector<std::vector<double>> one_p_rdm, two_p_rdm;
    sbd::tpb::diag(
        comm, sbd_data.inner, fcidump, adet, bdet, loadname, savename, energy, density,
        co_adet, co_bdet, one_p_rdm, two_p_rdm
    );

    // if (mpi_rank == master_rank) {
    //     if (one_p_rdm.size() != 0) {
    //         std::cout << " Start calculating 1pRDM and 2pRDM." << std::endl;

    //        double onebody = 0.0;
    //        double twobody = 0.0;
    //        double I0;
    //        sbd::oneInt<double> I1;
    //        sbd::twoInt<double> I2;
    //        sbd::SetupIntegrals(fcidump, L, N, I0, I1, I2);

    //        std::ofstream ofs_one(sbd_data.output_dir / sbd_data.one_RDM_file);
    //        ofs_one.precision(16);
    //        for (int io = 0; io < L; io++) {
    //            for (int jo = 0; jo < L; jo++) {
    //                ofs_one << io << " " << jo << " "
    //                        << one_p_rdm[0][io + L * jo] + one_p_rdm[1][io + L * jo]
    //                        << std::endl;
    //                onebody += I1.Value(2 * io, 2 * jo) *
    //                           (one_p_rdm[0][io + L * jo] + one_p_rdm[1][io + L *
    //                           jo]);
    //            }
    //        }

    //        std::ofstream ofs_two(sbd_data.output_dir / sbd_data.two_RDM_file);
    //        ofs_two.precision(16);
    //        for (int io = 0; io < L; io++) {
    //            for (int jo = 0; jo < L; jo++) {
    //                for (int ia = 0; ia < L; ia++) {
    //                    for (int ja = 0; ja < L; ja++) {
    //                        ofs_two
    //                            << io << " " << jo << " " << ia << " " << ja << " "
    //                            << two_p_rdm[0][io + L * jo + L * L * (ia + L * ja)] +
    //                                   two_p_rdm[1]
    //                                            [io + L * jo + L * L * (ia + L * ja)]
    //                                            +
    //                                   two_p_rdm[2]
    //                                            [io + L * jo + L * L * (ia + L * ja)]
    //                                            +
    //                                   two_p_rdm[3][io + L * jo + L * L * (ia + L *
    //                                   ja)]
    //                            << std::endl;
    //                        twobody +=
    //                            0.5 * I2.Value(2 * io, 2 * ia, 2 * jo, 2 * ja) *
    //                            two_p_rdm[0][io + L * jo + L * L * ia + L * L * L *
    //                            ja];
    //                        twobody +=
    //                            0.5 * I2.Value(2 * io, 2 * ia, 2 * jo, 2 * ja) *
    //                            two_p_rdm[1][io + L * jo + L * L * ia + L * L * L *
    //                            ja];
    //                        twobody +=
    //                            0.5 * I2.Value(2 * io, 2 * ia, 2 * jo, 2 * ja) *
    //                            two_p_rdm[2][io + L * jo + L * L * ia + L * L * L *
    //                            ja];
    //                        twobody +=
    //                            0.5 * I2.Value(2 * io, 2 * ia, 2 * jo, 2 * ja) *
    //                            two_p_rdm[3][io + L * jo + L * L * ia + L * L * L *
    //                            ja];
    //                    }
    //                }
    //            }
    //        }

    //        std::cout << " One-Body energy = " << onebody << std::endl;
    //        std::cout << " Two-Body energy = " << twobody << std::endl;
    //        std::cout << " One-Body + Two-Body energy = " << onebody + twobody
    //                  << std::endl;
    //    }
    //}

    return {energy, density};
}

#endif
