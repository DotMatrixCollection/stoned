require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "index")
require File.join(BUNDLER_RB_DIR, "bundler", "lazy_specification")
require File.join(BUNDLER_RB_DIR, "bundler", "spec_set")

module Bundler
  class Resolver
    def initialize(dependencies, index = nil)
      @dependencies = dependencies
      @index = index
    end

    def start
      specs = @dependencies.map do |dep|
        matches = if @index && @index.respond_to?(:search)
          @index.search(dep)
        else
          []
        end
        match = matches[0]
        if match
          match
        else
          Bundler::LazySpecification.new(dep.name, "0", dep.source)
        end
      end
      Bundler::SpecSet.new(specs)
    end
  end
end
