require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "spec_set")

module Bundler
  class Materialization
    attr_reader :dependencies, :specs

    def initialize(dependencies, specs)
      @dependencies = dependencies
      @specs = specs.is_a?(Bundler::SpecSet) ? specs : Bundler::SpecSet.new(specs)
    end

    def complete?
      !@specs.empty?
    end
  end
end
