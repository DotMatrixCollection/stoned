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

    def path_sources
      @sources.select { |source| source.class.name == "Bundler::Source::Path" }
    end

    def git_sources
      @sources.select { |source| source.class.name == "Bundler::Source::Git" }
    end
  end
end
