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

    def each(&block)
      @sources.each(&block)
    end

    def size
      @sources.length
    end

    def empty?
      @sources.empty?
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

    def metadata_source
      @sources.find { |source| source.class.name == "Bundler::Source::Metadata" }
    end

    def installed_source
      @sources.find { |source| source.class.name == "Bundler::Source::Installed" }
    end

    def default_source
      rubygems_sources[0] || path_sources[0] || git_sources[0]
    end
  end
end
