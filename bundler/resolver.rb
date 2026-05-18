require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "spec_set")

module Bundler
  class Resolver
    def initialize(dependencies, index = nil)
      @dependencies = dependencies
      @index = index
    end

    def start
      specs = @dependencies.map do |dep|
        Bundler::LazySpecification.new(dep.name, "0", dep.source)
      end
      Bundler::SpecSet.new(specs)
    end
  end
end
