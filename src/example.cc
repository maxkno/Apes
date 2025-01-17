#include "Channel/ChannelNode.hh"
#include "Channel/Integrand.hh"
#include "Channel/MultiChannel.hh"
#include "Model/Model.hh"
#include "Channel/Channel.hh"
#include "Tools/JetCluster.hh"
#include <iostream>

std::vector<int> combine(int i, int j) { 
    if((i == 21 && j == 23) || (i == 23 && j == 21)) return {};
    if(i == 21 && j == 21) return {21};
    if(i == -j) {
        return {21, 23};
    } else if (i != j) {
        if(i == 21 || i == 23) {
            return {j};
        } else if(j == 21 || j == 23) {
            return {i};
        }
    }
    return {};
}

bool PreProcess(const std::vector<apes::FourVector> &mom) {
    if(std::isnan(mom[0][0])) {
        spdlog::trace("Failed inital state");
        return false;
    }
    if((mom[0]+mom[1]).Mass2() > 13000*13000) {
        spdlog::trace("Failed s limit");
        return false;
    }
    apes::JetCluster cluster(0.4);
    auto jets = cluster(mom);
    if(jets.size() < mom.size()) return false;
    return true;
    for(size_t i = 2; i < mom.size(); ++i) {
        if(mom[i].Pt() < 30) return false;
        if(std::abs(mom[i].Rapidity()) > 5) {
            spdlog::trace("Failed y cut for {}: y = {}", i, mom[i].Rapidity());
            return false;
        }
        for(size_t j = i+1; j < mom.size(); ++j) {
            if(mom[i].DeltaR(mom[j]) < 0.4) {
                spdlog::trace("Failed R cut for {},{}: DR = {}", i, j, mom[i].DeltaR(mom[j]));
                return false;
            }
        }
    }
    return true;
}

bool PostProcess(const std::vector<apes::FourVector>&, double) { return true; }

int main() {
    apes::Model model(combine);
    model.Mass(1) = 0;
    model.Mass(-1) = 0;
    model.Mass(2) = 0;
    model.Mass(-2) = 0;
    model.Mass(3) = 0;
    model.Mass(-3) = 0;
    model.Mass(23) = 100;
    model.Mass(21) = 0;
    model.Width(1) = 0;
    model.Width(-1) = 0;
    model.Width(2) = 0;
    model.Width(-2) = 0;
    model.Width(3) = 0;
    model.Width(-3) = 0;
    model.Width(23) = 2.5;
    model.Width(21) = 0;
    model.Mass(5) = 172;
    model.Width(5) = 5;
    model.Mass(-5) = 172;
    model.Width(-5) = 5;

    spdlog::set_level(spdlog::level::trace);

    std::vector<std::vector<int>> processes{
        {2, -2, 1, -1},
        {-1, -2, 1, 2},
        {1, -1, 2, -2, 2, -2},
        {1, -1, 2, -2, 3, -3},
        {1, -1, 1, -1, 21, 21, 21},
        {1, -1, 2, -2, 21, 21, 21}
    };

    // Construct channels
    // for(const auto &process : processes) {
    //     auto mappings = apes::ConstructChannels(13000, process, model, 0);
    //     if(mappings.size() == 0)
    //         throw std::logic_error(fmt::format("Failed process {{{}}}",
    //                                            fmt::join(process.begin(), process.end(), ", ")));
    // }

    auto mappings = apes::ConstructChannels(13000, {21, 21, 21, 21}, model, 1);

    // Setup integrator
    apes::Integrand<apes::FourVector> integrand;
    for(auto &mapping : mappings) {
        apes::Channel<apes::FourVector> channel;
        channel.mapping = std::move(mapping);
        // Initializer takes the number of integration dimensions
        // and the number of bins for vegas to start with
        apes::AdaptiveMap map(channel.mapping -> NDims(), 2);
        // Initializer takes adaptive map and settings (found in struct VegasParams)
        channel.integrator = apes::Vegas(map, apes::VegasParams{});
        integrand.AddChannel(std::move(channel));
    }

    // Initialize the multichannel integrator
    // Takes the number of dimensions, the number of channels, and options
    // The options can be found in the struct MultiChannelParams
    apes::MultiChannel integrator{integrand.NDims(), integrand.NChannels(), {}};

    // To integrate a function you need to pass it in and tell it to optimize
    // Summary will print out a summary of the results including the values of alpha
    auto func = [&](const std::vector<apes::FourVector> &) {
        return 1; // (pow(p[0]*p[2], 2)+pow(p[0]*p[3], 2))/pow(p[2]*p[3], 2);
    };
    integrand.Function() = func;
    integrand.PreProcess() = PreProcess;
    integrand.PostProcess() = PostProcess;
    spdlog::info("Starting optimization");
    integrator.Optimize(integrand);
    integrator.Summary();
    integrator(integrand); // Generate events

    return 0;
}
