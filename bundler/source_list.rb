require "bundler"

module Bundler
  class SourceList
    def initialize
      @sources = []
    end

    def add_source(source)
      @sources << source
      source
    end

    def all_sources
      @sources
    end

    def rubygems_sources
      @sources.select { |source| source.class.name == "Bundler::Source::Rubygems" }
    end
  end
end
