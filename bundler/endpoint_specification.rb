require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "lazy_specification")

module Bundler
  class EndpointSpecification < LazySpecification
    attr_reader :dependencies

    def initialize(name, version, source = nil, dependencies = [])
      super(name, version, source)
      @dependencies = dependencies
    end
  end
end
